/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "shared/offline_compiler/source/ocloc_error_code.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"

#include "igfxfmid.h"
#include "platforms.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace NEO {
bool requestedFatBinary(const std::vector<std::string> &args, OclocArgHelper *helper) {
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            ConstStringRef deviceArg(args[argIndex + 1]);
            auto deviceName = deviceArg.str();
            ProductConfigHelper::adjustDeviceName(deviceName);

            auto retVal = deviceArg.contains("*");
            retVal |= deviceArg.contains(":");
            retVal |= deviceArg.contains(",");
            retVal |= helper->productConfigHelper->isFamily(deviceName);
            retVal |= helper->productConfigHelper->isRelease(deviceName);

            return retVal;
        }
    }
    return false;
}

template <>
void getProductsAcronymsForTarget<AOT::FAMILY>(std::vector<NEO::ConstStringRef> &out, AOT::FAMILY target, OclocArgHelper *argHelper) {
    auto allSuppportedProducts = argHelper->productConfigHelper->getDeviceAotInfo();
    for (const auto &device : allSuppportedProducts) {
        if (device.family == target && !device.acronyms.empty()) {
            if (std::find(out.begin(), out.end(), device.acronyms.front()) == out.end()) {
                out.push_back(device.acronyms.front());
            }
        }
    }
}

template <>
void getProductsAcronymsForTarget<AOT::RELEASE>(std::vector<NEO::ConstStringRef> &out, AOT::RELEASE target, OclocArgHelper *argHelper) {
    auto allSuppportedProducts = argHelper->productConfigHelper->getDeviceAotInfo();
    for (const auto &device : allSuppportedProducts) {
        if (device.release == target && !device.acronyms.empty()) {
            if (std::find(out.begin(), out.end(), device.acronyms.front()) == out.end()) {
                out.push_back(device.acronyms.front());
            }
        }
    }
}

template <typename T>
void getProductsForTargetRange(T targetFrom, T targetTo, std::vector<ConstStringRef> &out,
                               OclocArgHelper *argHelper) {
    if (targetFrom > targetTo) {
        std::swap(targetFrom, targetTo);
    }
    while (targetFrom <= targetTo) {
        getProductsAcronymsForTarget<T>(out, targetFrom, argHelper);
        targetFrom = static_cast<T>(static_cast<unsigned int>(targetFrom) + 1);
    }
}

void getProductsForRange(unsigned int productFrom, unsigned int productTo, std::vector<ConstStringRef> &out,
                         OclocArgHelper *argHelper) {
    auto allSuppportedProducts = argHelper->productConfigHelper->getDeviceAotInfo();

    for (const auto &device : allSuppportedProducts) {
        auto validAcronym = device.aotConfig.ProductConfig >= productFrom;
        validAcronym &= device.aotConfig.ProductConfig <= productTo;
        validAcronym &= !device.acronyms.empty();
        if (validAcronym) {
            out.push_back(device.acronyms.front());
        }
    }
}

std::vector<ConstStringRef> getProductForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, OclocArgHelper *argHelper) {
    std::vector<ConstStringRef> requestedProducts = {};
    auto rangeToStr = rangeTo.str();
    auto rangeFromStr = rangeFrom.str();

    ProductConfigHelper::adjustDeviceName(rangeToStr);
    ProductConfigHelper::adjustDeviceName(rangeFromStr);

    if (argHelper->productConfigHelper->isFamily(rangeFromStr) && argHelper->productConfigHelper->isFamily(rangeToStr)) {
        auto familyFrom = ProductConfigHelper::getFamilyForAcronym(rangeFromStr);
        auto familyTo = ProductConfigHelper::getFamilyForAcronym(rangeToStr);
        getProductsForTargetRange(familyFrom, familyTo, requestedProducts, argHelper);

    } else if (argHelper->productConfigHelper->isRelease(rangeFromStr) && argHelper->productConfigHelper->isRelease(rangeToStr)) {
        auto releaseFrom = ProductConfigHelper::getReleaseForAcronym(rangeFromStr);
        auto releaseTo = ProductConfigHelper::getReleaseForAcronym(rangeToStr);
        getProductsForTargetRange(releaseFrom, releaseTo, requestedProducts, argHelper);

    } else if (argHelper->productConfigHelper->isProductConfig(rangeFromStr) && argHelper->productConfigHelper->isProductConfig(rangeToStr)) {
        unsigned int productConfigFrom = ProductConfigHelper::getProductConfigForAcronym(rangeFromStr);
        unsigned int productConfigTo = ProductConfigHelper::getProductConfigForAcronym(rangeToStr);
        if (productConfigFrom > productConfigTo) {
            std::swap(productConfigFrom, productConfigTo);
        }
        getProductsForRange(productConfigFrom, productConfigTo, requestedProducts, argHelper);
    } else {
        auto target = rangeFromStr + ":" + rangeToStr;
        argHelper->printf("Failed to parse target : %s.\n", target.c_str());
        return {};
    }

    return requestedProducts;
}

std::vector<ConstStringRef> getProductForOpenRange(ConstStringRef openRange, OclocArgHelper *argHelper, bool rangeTo) {
    std::vector<ConstStringRef> requestedProducts = {};
    auto openRangeStr = openRange.str();
    ProductConfigHelper::adjustDeviceName(openRangeStr);

    if (argHelper->productConfigHelper->isFamily(openRangeStr)) {
        auto family = ProductConfigHelper::getFamilyForAcronym(openRangeStr);
        if (rangeTo) {
            unsigned int familyFrom = AOT::UNKNOWN_FAMILY;
            ++familyFrom;
            getProductsForTargetRange(static_cast<AOT::FAMILY>(familyFrom), family, requestedProducts, argHelper);
        } else {
            unsigned int familyTo = AOT::FAMILY_MAX;
            --familyTo;
            getProductsForTargetRange(family, static_cast<AOT::FAMILY>(familyTo), requestedProducts, argHelper);
        }
    } else if (argHelper->productConfigHelper->isRelease(openRangeStr)) {
        auto release = ProductConfigHelper::getReleaseForAcronym(openRangeStr);
        if (rangeTo) {
            unsigned int releaseFrom = AOT::UNKNOWN_FAMILY;
            ++releaseFrom;
            getProductsForTargetRange(static_cast<AOT::RELEASE>(releaseFrom), release, requestedProducts, argHelper);
        } else {
            unsigned int releaseTo = AOT::RELEASE_MAX;
            --releaseTo;
            getProductsForTargetRange(release, static_cast<AOT::RELEASE>(releaseTo), requestedProducts, argHelper);
        }
    } else if (argHelper->productConfigHelper->isProductConfig(openRangeStr)) {
        auto product = ProductConfigHelper::getProductConfigForAcronym(openRangeStr);
        if (rangeTo) {
            unsigned int productFrom = AOT::UNKNOWN_ISA;
            ++productFrom;
            getProductsForRange(productFrom, static_cast<unsigned int>(product), requestedProducts, argHelper);
        } else {
            unsigned int productTo = AOT::CONFIG_MAX_PLATFORM;
            --productTo;
            getProductsForRange(product, static_cast<AOT::PRODUCT_CONFIG>(productTo), requestedProducts, argHelper);
        }
    }
    return requestedProducts;
}

std::vector<ConstStringRef> getProductForSpecificTarget(CompilerOptions::TokenizedString targets, OclocArgHelper *argHelper) {
    std::vector<ConstStringRef> requestedConfigs;
    for (const auto &target : targets) {
        auto targetStr = target.str();
        ProductConfigHelper::adjustDeviceName(targetStr);

        if (argHelper->productConfigHelper->isFamily(targetStr)) {
            auto family = ProductConfigHelper::getFamilyForAcronym(targetStr);
            getProductsAcronymsForTarget(requestedConfigs, family, argHelper);
        } else if (argHelper->productConfigHelper->isRelease(targetStr)) {
            auto release = ProductConfigHelper::getReleaseForAcronym(targetStr);
            getProductsAcronymsForTarget(requestedConfigs, release, argHelper);
        } else if (argHelper->productConfigHelper->isProductConfig(targetStr)) {
            requestedConfigs.push_back(target);
        } else {
            argHelper->printf("Failed to parse target : %s - invalid device:\n", target.str().c_str());
            return {};
        }
    }
    return requestedConfigs;
}

std::vector<ConstStringRef> getTargetProductsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper) {
    std::vector<ConstStringRef> retVal;
    if (deviceArg == "*") {
        return argHelper->productConfigHelper->getRepresentativeProductAcronyms();
    } else {
        auto sets = CompilerOptions::tokenize(deviceArg, ',');
        if (sets[0].contains(":")) {
            auto range = CompilerOptions::tokenize(deviceArg, ':');
            if (range.size() > 2) {
                argHelper->printf("Invalid range : %s - should be from:to or :to or from:\n", sets[0].str().c_str());
                return {};
            }
            if (range.size() == 1) {
                bool rangeTo = (':' == sets[0][0]);

                retVal = getProductForOpenRange(range[0], argHelper, rangeTo);

            } else {
                retVal = getProductForClosedRange(range[0], range[1], argHelper);
            }
        } else {
            retVal = getProductForSpecificTarget(sets, argHelper);
        }
    }
    return retVal;
}

int buildFatBinaryForTarget(int retVal, const std::vector<std::string> &argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                            OfflineCompiler *pCompiler, OclocArgHelper *argHelper, const std::string &product) {

    if (retVal == 0) {
        retVal = buildWithSafetyGuard(pCompiler);
        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            argHelper->printf("%s\n", buildLog.c_str());
        }
        if (retVal == 0) {
            if (!pCompiler->isQuiet())
                argHelper->printf("Build succeeded for : %s.\n", product.c_str());
        } else {
            argHelper->printf("Build failed for : %s with error code: %d\n", product.c_str(), retVal);
            argHelper->printf("Command was:");
            for (const auto &arg : argsCopy)
                argHelper->printf(" %s", arg.c_str());
            argHelper->printf("\n");
        }
    }
    if (retVal) {
        return retVal;
    }

    std::string productConfig("");
    if (product.find(".") != std::string::npos) {
        productConfig = product;
    } else {
        productConfig = ProductConfigHelper::parseMajorMinorRevisionValue(ProductConfigHelper::getProductConfigForAcronym(product));
    }

    fatbinary.appendFileEntry(pointerSize + "." + productConfig, pCompiler->getPackedDeviceBinaryOutput());
    return retVal;
}

int buildFatBinary(const std::vector<std::string> &args, OclocArgHelper *argHelper) {
    std::string pointerSizeInBits = (sizeof(void *) == 4) ? "32" : "64";
    size_t deviceArgIndex = -1;
    std::string inputFileName = "";
    std::string outputFileName = "";
    std::string outputDirectory = "";
    bool spirvInput = false;
    bool excludeIr = false;

    std::vector<std::string> argsCopy(args);
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            deviceArgIndex = argIndex + 1;
            ++argIndex;
        } else if ((CompilerOptions::arch32bit == currArg) || (ConstStringRef("-32") == currArg)) {
            pointerSizeInBits = "32";
        } else if ((CompilerOptions::arch64bit == currArg) || (ConstStringRef("-64") == currArg)) {
            pointerSizeInBits = "64";
        } else if ((ConstStringRef("-file") == currArg) && hasMoreArgs) {
            inputFileName = args[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-output") == currArg) && hasMoreArgs) {
            outputFileName = args[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-out_dir") == currArg) && hasMoreArgs) {
            outputDirectory = args[argIndex + 1];
            ++argIndex;
        } else if (ConstStringRef("-exclude_ir") == currArg) {
            excludeIr = true;
        } else if (ConstStringRef("-spirv_input") == currArg) {
            spirvInput = true;
        }
    }

    const bool shouldPreserveGenericIr = spirvInput && !excludeIr;
    if (shouldPreserveGenericIr) {
        argsCopy.push_back("-exclude_ir");
    }

    if (deviceArgIndex == static_cast<size_t>(-1)) {
        argHelper->printf("Error! Command does not contain device argument!\n");
        return OclocErrorCode::INVALID_COMMAND_LINE;
    }

    Ar::ArEncoder fatbinary(true);
    std::vector<ConstStringRef> targetProducts;
    targetProducts = getTargetProductsForFatbinary(ConstStringRef(args[deviceArgIndex]), argHelper);
    if (targetProducts.empty()) {
        argHelper->printf("Failed to parse target devices from : %s\n", args[deviceArgIndex].c_str());
        return 1;
    }
    for (const auto &product : targetProducts) {
        int retVal = 0;
        argsCopy[deviceArgIndex] = product.str();

        std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(argsCopy.size(), argsCopy, false, retVal, argHelper)};
        if (OclocErrorCode::SUCCESS != retVal) {
            argHelper->printf("Error! Couldn't create OfflineCompiler. Exiting.\n");
            return retVal;
        }

        retVal = buildFatBinaryForTarget(retVal, argsCopy, pointerSizeInBits, fatbinary, pCompiler.get(), argHelper, product.str());
        if (retVal) {
            return retVal;
        }
    }

    if (shouldPreserveGenericIr) {
        const auto errorCode = appendGenericIr(fatbinary, inputFileName, argHelper);
        if (errorCode != OclocErrorCode::SUCCESS) {
            argHelper->printf("Error! Couldn't append generic IR file!\n");
            return errorCode;
        }
    }

    auto fatbinaryData = fatbinary.encode();
    std::string fatbinaryFileName = outputFileName;
    if (outputFileName.empty() && (false == inputFileName.empty())) {
        fatbinaryFileName = OfflineCompiler::getFileNameTrunk(inputFileName) + ".ar";
    }
    if (false == outputDirectory.empty()) {
        fatbinaryFileName = outputDirectory + "/" + outputFileName;
    }
    argHelper->saveOutput(fatbinaryFileName, fatbinaryData.data(), fatbinaryData.size());

    return 0;
}

int appendGenericIr(Ar::ArEncoder &fatbinary, const std::string &inputFile, OclocArgHelper *argHelper) {
    std::size_t fileSize = 0;
    std::unique_ptr<char[]> fileContents = argHelper->loadDataFromFile(inputFile, fileSize);
    if (fileSize == 0) {
        argHelper->printf("Error! Couldn't read input file!\n");
        return OclocErrorCode::INVALID_FILE;
    }

    const auto ir = ArrayRef<const uint8_t>::fromAny(fileContents.get(), fileSize);
    if (!isSpirVBitcode(ir)) {
        argHelper->printf("Error! Input file is not in supported generic IR format! "
                          "Currently supported format is SPIR-V.\n");
        return OclocErrorCode::INVALID_FILE;
    }

    const auto encodedElf = createEncodedElfWithSpirv(ir);
    ArrayRef<const uint8_t> genericIrFile{encodedElf.data(), encodedElf.size()};

    fatbinary.appendFileEntry("generic_ir", genericIrFile);
    return OclocErrorCode::SUCCESS;
}

std::vector<uint8_t> createEncodedElfWithSpirv(const ArrayRef<const uint8_t> &spirv) {
    using namespace NEO::Elf;
    ElfEncoder<EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = ET_OPENCL_OBJECTS;
    elfEncoder.appendSection(SHT_OPENCL_SPIRV, SectionNamesOpenCl::spirvObject, spirv);

    return elfEncoder.encode();
}

} // namespace NEO