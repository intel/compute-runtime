/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"

#include "compiler_options.h"
#include "igfxfmid.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace NEO {

bool requestedFatBinary(int argc, const char *argv[]) {
    for (int argIndex = 1; argIndex < argc; argIndex++) {
        const auto &currArg = argv[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < argc);
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            ConstStringRef deviceArg(argv[argIndex + 1], strlen(argv[argIndex + 1]));
            return deviceArg.contains("*") || deviceArg.contains("-") || deviceArg.contains(",") || deviceArg.contains("gen");
        }
    }
    return false;
}

std::vector<PRODUCT_FAMILY> getAllSupportedTargetPlatforms() {
    return std::vector<PRODUCT_FAMILY>{ALL_SUPPORTED_PRODUCT_FAMILIES};
}

std::vector<ConstStringRef> toProductNames(const std::vector<PRODUCT_FAMILY> &productIds) {
    std::vector<ConstStringRef> ret;
    for (auto prodId : productIds) {
        ret.push_back(ConstStringRef(hardwarePrefix[prodId], strlen(hardwarePrefix[prodId])));
    }
    return ret;
}

PRODUCT_FAMILY asProductId(ConstStringRef product, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms) {
    for (auto family : allSupportedPlatforms) {
        if (product == hardwarePrefix[family]) {
            return family;
        }
    }
    return IGFX_UNKNOWN;
}

GFXCORE_FAMILY asGfxCoreId(ConstStringRef core) {
    ConstStringRef coreIgnoreG(core.begin() + 1, core.size() - 1);
    for (unsigned int coreId = 0; coreId < IGFX_MAX_CORE; ++coreId) {
        if (nullptr == familyName[coreId]) {
            continue;
        }
        if (ConstStringRef(familyName[coreId] + 1, strlen(familyName[coreId]) - 1) == coreIgnoreG) {
            return static_cast<GFXCORE_FAMILY>(coreId);
        }
    }

    return IGFX_UNKNOWN_CORE;
}

void appendPlatformsForGfxCore(GFXCORE_FAMILY core, const std::vector<PRODUCT_FAMILY> &allSupportedPlatforms, std::vector<PRODUCT_FAMILY> &out) {
    for (auto family : allSupportedPlatforms) {
        if (core == hardwareInfoTable[family]->platform.eRenderCoreFamily) {
            out.push_back(family);
        }
    }
}

std::vector<ConstStringRef> getTargetPlatformsForFatbinary(ConstStringRef deviceArg) {
    std::vector<PRODUCT_FAMILY> allSupportedPlatforms = getAllSupportedTargetPlatforms();
    if (deviceArg == "*") {
        return toProductNames(allSupportedPlatforms);
    }

    auto genArg = ConstStringRef("gen");

    std::vector<PRODUCT_FAMILY> requestedPlatforms;
    auto sets = CompilerOptions::tokenize(deviceArg, ',');
    for (auto set : sets) {
        if (set.contains("-")) {
            auto range = CompilerOptions::tokenize(deviceArg, '-');
            if (range.size() > 2) {
                printf("Invalid range : %s - should be from-to or -to or from-\n", set.str().c_str());
                return {};
            }

            if (range.size() == 1) {
                // open range , from-max or min-to
                if (range[0].contains("gen")) {
                    auto coreId = asGfxCoreId(range[0]);
                    if (IGFX_UNKNOWN_CORE == coreId) {
                        printf("Unknown device : %s\n", set.str().c_str());
                        return {};
                    }
                    if ('-' == set[0]) {
                        // to
                        unsigned int coreIt = IGFX_UNKNOWN_CORE;
                        ++coreIt;
                        while (coreIt <= static_cast<unsigned int>(coreId)) {
                            appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(coreIt), allSupportedPlatforms, requestedPlatforms);
                            ++coreIt;
                        }
                    } else {
                        // from
                        unsigned int coreIt = coreId;
                        while (coreIt < static_cast<unsigned int>(IGFX_MAX_CORE)) {
                            appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(coreIt), allSupportedPlatforms, requestedPlatforms);
                            ++coreIt;
                        }
                    }
                } else {
                    auto prodId = asProductId(range[0], allSupportedPlatforms);
                    if (IGFX_UNKNOWN == prodId) {
                        printf("Unknown device : %s\n", range[0].str().c_str());
                        return {};
                    }
                    auto prodIt = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), prodId);
                    assert(prodIt != allSupportedPlatforms.end());
                    if ('-' == set[0]) {
                        // to
                        requestedPlatforms.insert(requestedPlatforms.end(), allSupportedPlatforms.begin(), prodIt + 1);
                    } else {
                        // from
                        requestedPlatforms.insert(requestedPlatforms.end(), prodIt, allSupportedPlatforms.end());
                    }
                }
            } else {
                if (range[0].contains("gen")) {
                    if (false == range[1].contains("gen")) {
                        printf("Ranges mixing platforms and gfxCores is not supported : %s - should be genFrom-genTo or platformFrom-platformTo\n", set.str().c_str());
                        return {};
                    }
                    auto coreFrom = asGfxCoreId(range[0]);
                    auto coreTo = asGfxCoreId(range[1]);
                    if (IGFX_UNKNOWN_CORE == coreFrom) {
                        printf("Unknown device : %s\n", set.str().c_str());
                        return {};
                    }
                    if (IGFX_UNKNOWN_CORE == coreTo) {
                        printf("Unknown device : %s\n", set.str().c_str());
                        return {};
                    }
                    if (coreFrom > coreTo) {
                        std::swap(coreFrom, coreTo);
                    }
                    while (coreFrom <= coreTo) {
                        appendPlatformsForGfxCore(static_cast<GFXCORE_FAMILY>(coreFrom), allSupportedPlatforms, requestedPlatforms);
                        coreFrom = static_cast<GFXCORE_FAMILY>(static_cast<unsigned int>(coreFrom) + 1);
                    }
                } else {
                    auto platformFrom = asProductId(range[0], allSupportedPlatforms);
                    auto platformTo = asProductId(range[1], allSupportedPlatforms);
                    if (IGFX_UNKNOWN == platformFrom) {
                        printf("Unknown device : %s\n", set.str().c_str());
                        return {};
                    }
                    if (IGFX_UNKNOWN == platformTo) {
                        printf("Unknown device : %s\n", set.str().c_str());
                        return {};
                    }
                    if (platformFrom > platformTo) {
                        std::swap(platformFrom, platformTo);
                    }

                    auto from = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), platformFrom);
                    auto to = std::find(allSupportedPlatforms.begin(), allSupportedPlatforms.end(), platformTo) + 1;
                    requestedPlatforms.insert(requestedPlatforms.end(), from, to);
                }
            }
        } else if (set.contains("gen")) {
            if (set.size() == genArg.size()) {
                printf("Invalid gen-based device : %s - gen should be followed by a number\n", set.str().c_str());
            } else {
                auto coreId = asGfxCoreId(set);
                if (IGFX_UNKNOWN_CORE == coreId) {
                    printf("Unknown device : %s\n", set.str().c_str());
                    return {};
                }
                appendPlatformsForGfxCore(coreId, allSupportedPlatforms, requestedPlatforms);
            }
        } else {
            auto prodId = asProductId(set, allSupportedPlatforms);
            if (IGFX_UNKNOWN == prodId) {
                printf("Unknown device : %s\n", set.str().c_str());
                return {};
            }
            requestedPlatforms.push_back(prodId);
        }
    }
    return toProductNames(requestedPlatforms);
}

int buildFatbinary(int argc, const char *argv[], OclocArgHelper *helper) {
    std::string pointerSizeInBits = (sizeof(void *) == 4) ? "32" : "64";
    int deviceArgIndex = -1;
    std::string inputFileName = "";
    std::string outputFileName = "";
    std::string outputDirectory = "";

    std::vector<std::string> argsCopy;
    if (argc > 1) {
        argsCopy.assign(argv, argv + argc);
    }

    for (int argIndex = 1; argIndex < argc; argIndex++) {
        const auto &currArg = argv[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < argc);
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            deviceArgIndex = argIndex + 1;
            ++argIndex;
        } else if ((CompilerOptions::arch32bit == currArg) || (ConstStringRef("-32") == currArg)) {
            pointerSizeInBits = "32";
        } else if ((CompilerOptions::arch64bit == currArg) || (ConstStringRef("-64") == currArg)) {
            pointerSizeInBits = "64";
        } else if ((ConstStringRef("-file") == currArg) && hasMoreArgs) {
            inputFileName = argv[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-output") == currArg) && hasMoreArgs) {
            outputFileName = argv[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-out_dir") == currArg) && hasMoreArgs) {
            outputDirectory = argv[argIndex + 1];
            ++argIndex;
        }
    }

    std::vector<ConstStringRef> targetPlatforms;
    targetPlatforms = getTargetPlatformsForFatbinary(ConstStringRef(argv[deviceArgIndex], strlen(argv[deviceArgIndex])));
    if (targetPlatforms.empty()) {
        printf("Failed to parse target devices from : %s\n", argv[deviceArgIndex]);
        return 1;
    }

    NEO::Ar::ArEncoder fatbinary(true);

    for (auto targetPlatform : targetPlatforms) {
        int retVal = 0;
        argsCopy[deviceArgIndex] = targetPlatform.str();
        std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(argc, argsCopy, false, retVal, helper)};
        if (ErrorCode::SUCCESS != retVal) {
            printf("Error! Couldn't create OfflineCompiler. Exiting.\n");
            return retVal;
        }
        auto stepping = pCompiler->getHardwareInfo().platform.usRevId;
        if (retVal == 0) {
            retVal = buildWithSafetyGuard(pCompiler.get());

            std::string buildLog = pCompiler->getBuildLog();
            if (buildLog.empty() == false) {
                printf("%s\n", buildLog.c_str());
            }

            if (retVal == 0) {
                if (!pCompiler->isQuiet())
                    printf("Build succeeded for : %s.\n", (targetPlatform.str() + "." + std::to_string(stepping)).c_str());
            } else {
                printf("Build failed for : %s with error code: %d\n", (targetPlatform.str() + "." + std::to_string(stepping)).c_str(), retVal);
                printf("Command was:");
                for (auto i = 0; i < argc; ++i)
                    printf(" %s", argv[i]);
                printf("\n");
            }
        }

        if (0 != retVal) {
            return retVal;
        }

        fatbinary.appendFileEntry(pointerSizeInBits + "." + targetPlatform.str() + "." + std::to_string(stepping), pCompiler->getPackedDeviceBinaryOutput());
    }

    auto fatbinaryData = fatbinary.encode();
    std::string fatbinaryFileName = outputFileName;
    if (outputFileName.empty() && (false == inputFileName.empty())) {
        fatbinaryFileName = OfflineCompiler::getFileNameTrunk(inputFileName) + ".ar";
    }
    if (false == outputDirectory.empty()) {
        fatbinaryFileName = outputDirectory + "/" + outputFileName;
    }
    helper->saveOutput(fatbinaryFileName, fatbinaryData.data(), fatbinaryData.size());

    return 0;
}

} // namespace NEO
