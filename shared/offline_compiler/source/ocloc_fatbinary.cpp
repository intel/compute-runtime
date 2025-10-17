/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_fatbinary.h"

#include "shared/offline_compiler/source/ocloc_api.h"
#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/offline_compiler/source/ocloc_interface.h"
#include "shared/offline_compiler/source/offline_compiler.h"
#include "shared/offline_compiler/source/utilities/safety_caller.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/intermediate_representations.h"
#include "shared/source/compiler_interface/tokenized_string.h"
#include "shared/source/device_binary_format/ar/ar_encoder.h"
#include "shared/source/device_binary_format/elf/elf_encoder.h"
#include "shared/source/device_binary_format/elf/ocl_elf.h"
#include "shared/source/helpers/file_io.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/helpers/product_config_helper_former.h"
#include "shared/source/utilities/directory.h"

#include "neo_aot_platforms.h"
#include "neo_igfxfmid.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <set>

namespace NEO {

using Position = FormerProductConfigHelper::Position;

bool isSpvOnly(const std::vector<std::string> &args) {
    return std::find(args.begin(), args.end(), "-spv_only") != args.end();
}

bool requestedFatBinary(ConstStringRef deviceArg, OclocArgHelper *helper) {
    auto &prodHelper = *helper->productConfigHelper;
    auto &formerProdHelper = *helper->formerProductConfigHelper;
    auto deviceName = deviceArg.str();
    ProductConfigHelper::adjustDeviceName(deviceName);

    auto release = prodHelper.getReleaseFromDeviceName(deviceName);
    auto family = prodHelper.getFamilyFromDeviceName(deviceName);
    auto retVal = deviceArg.contains("*");
    retVal |= deviceArg.contains(":");
    retVal |= deviceArg.contains(",");
    retVal |= family != AOT::UNKNOWN_FAMILY;
    retVal |= release != AOT::UNKNOWN_RELEASE;

    retVal |= formerProdHelper.isSupportedTarget<AOT::FAMILY>(deviceName);
    retVal |= formerProdHelper.isSupportedTarget<AOT::RELEASE>(deviceName);

    return retVal;
}

bool requestedFatBinary(const std::vector<std::string> &args, OclocArgHelper *helper) {
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            return requestedFatBinary(args[argIndex + 1], helper);
        }
    }
    return false;
}

template <>
void getProductsAcronymsForTarget<AOT::RELEASE>(std::vector<NEO::ConstStringRef> &out, AOT::RELEASE target, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &allSuppportedProducts = prodHelper.getDeviceAotInfo();
    auto hasDeviceAcronym = std::any_of(allSuppportedProducts.begin(), allSuppportedProducts.end(), ProductConfigHelper::findDeviceAcronymForRelease(target));
    for (const auto &device : allSuppportedProducts) {
        if (device.release == target) {
            ConstStringRef acronym{};
            if (hasDeviceAcronym) {
                if (!device.deviceAcronyms.empty()) {
                    acronym = device.deviceAcronyms.front();
                }
            } else {
                if (!device.rtlIdAcronyms.empty()) {
                    acronym = device.rtlIdAcronyms.front();
                }
            }
            if (!acronym.empty() && std::find(out.begin(), out.end(), acronym) == out.end()) {
                out.push_back(acronym);
            }
        }
    }
}

template <>
void getProductsAcronymsForTarget<AOT::FAMILY>(std::vector<NEO::ConstStringRef> &out, AOT::FAMILY target, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &allSuppportedProducts = prodHelper.getDeviceAotInfo();
    std::vector<AOT::RELEASE> releases{};
    for (const auto &device : allSuppportedProducts) {
        if (device.family == target && std::find(releases.begin(), releases.end(), device.release) == releases.end()) {
            releases.push_back(device.release);
        }
    }
    for (const auto &release : releases) {
        getProductsAcronymsForTarget<AOT::RELEASE>(out, release, argHelper);
    }
}

template <typename T>
std::vector<ConstStringRef> getProductsForTargetRange(T targetFrom, T targetTo, OclocArgHelper *argHelper, const T maxValue) {
    std::vector<ConstStringRef> ret{};
    if (targetFrom > targetTo) {
        std::swap(targetFrom, targetTo);
    }
    while (targetFrom <= targetTo && targetFrom < maxValue) {
        getProductsAcronymsForTarget<T>(ret, targetFrom, argHelper);
        targetFrom = static_cast<T>(static_cast<unsigned int>(targetFrom) + 1);
    }
    return ret;
}

std::vector<ConstStringRef> getProductsForRange(unsigned int productFrom, unsigned int productTo,
                                                OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;
    std::vector<ConstStringRef> ret = {};

    auto &formerData = formerProdHelper.getData();
    uint32_t ipVersionAdded = 0;
    for (const auto &[acronym, ipVersion] : formerData.acronyms) {
        auto validAcronym = ipVersion >= productFrom;
        validAcronym &= ipVersion <= productTo;
        if (validAcronym && ipVersion != ipVersionAdded) {
            ret.push_back(acronym.c_str());
            ipVersionAdded = ipVersion;
        }
    }
    auto &allSuppportedProducts = prodHelper.getDeviceAotInfo();
    for (const auto &device : allSuppportedProducts) {
        auto validAcronym = device.aotConfig.value >= productFrom;
        validAcronym &= device.aotConfig.value <= productTo;

        if (validAcronym) {
            if (!device.deviceAcronyms.empty()) {
                ret.push_back(device.deviceAcronyms.front());
            } else if (!device.rtlIdAcronyms.empty()) {
                ret.push_back(device.rtlIdAcronyms.front());
            }
        }
    }
    return ret;
}

std::vector<ConstStringRef> getProductsForFamilyClosedRange(const std::string &fromStr, const std::string &toStr, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;

    auto getFamilyConfigWithFallback = [&](const std::string &familyStr, FormerProductConfigHelper::Position pos) {
        auto family = prodHelper.getFamilyFromDeviceName(familyStr);
        AOT::PRODUCT_CONFIG config = AOT::UNKNOWN_ISA;
        if (family != AOT::UNKNOWN_FAMILY) {
            config = prodHelper.getLastProductConfigFromFamilyName(family);
        }
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromFamilyName(familyStr, pos);
        }
        return config;
    };

    auto familyFrom = prodHelper.getFamilyFromDeviceName(fromStr);
    auto familyTo = prodHelper.getFamilyFromDeviceName(toStr);
    if (familyFrom != AOT::UNKNOWN_FAMILY && familyTo != AOT::UNKNOWN_FAMILY) {
        return getProductsForTargetRange(familyFrom, familyTo, argHelper, AOT::FAMILY_MAX);
    }
    AOT::PRODUCT_CONFIG prodConfigFrom = getFamilyConfigWithFallback(fromStr, Position::firstItem);
    AOT::PRODUCT_CONFIG prodConfigTo = getFamilyConfigWithFallback(toStr, Position::lastItem);
    if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
        return getProductsForRange(prodConfigFrom, prodConfigTo, argHelper);
    }
    return {};
}

std::vector<ConstStringRef> getProductsForReleaseClosedRange(const std::string &fromStr, const std::string &toStr, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;

    auto getReleaseConfigWithFallback = [&](const std::string &releaseStr, FormerProductConfigHelper::Position pos) {
        auto release = prodHelper.getReleaseFromDeviceName(releaseStr);
        auto config = prodHelper.getLastProductConfigFromReleaseName(release);
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromReleaseName(releaseStr, pos);
        }
        return config;
    };

    auto releaseFrom = prodHelper.getReleaseFromDeviceName(fromStr);
    auto releaseTo = prodHelper.getReleaseFromDeviceName(toStr);
    if (releaseFrom != AOT::UNKNOWN_RELEASE && releaseTo != AOT::UNKNOWN_RELEASE) {
        return getProductsForTargetRange(releaseFrom, releaseTo, argHelper, AOT::RELEASE_MAX);
    }
    AOT::PRODUCT_CONFIG prodConfigFrom = getReleaseConfigWithFallback(fromStr, Position::firstItem);
    AOT::PRODUCT_CONFIG prodConfigTo = getReleaseConfigWithFallback(toStr, Position::lastItem);
    if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
        return getProductsForRange(prodConfigFrom, prodConfigTo, argHelper);
    }
    return {};
}

std::vector<ConstStringRef> getProductsForProductClosedRange(const std::string &fromStr, const std::string &toStr, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;

    auto getProductConfigWithFallback = [&](const std::string &deviceName) {
        auto config = prodHelper.getProductConfigFromDeviceName(deviceName);
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromDeviceName(deviceName);
        }
        return config;
    };

    AOT::PRODUCT_CONFIG prodConfigFrom = getProductConfigWithFallback(fromStr);
    AOT::PRODUCT_CONFIG prodConfigTo = getProductConfigWithFallback(toStr);
    if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
        if (prodConfigFrom > prodConfigTo) {
            std::swap(prodConfigFrom, prodConfigTo);
        }
        return getProductsForRange(prodConfigFrom, prodConfigTo, argHelper);
    }
    return {};
}

std::vector<ConstStringRef> getProductForClosedRange(ConstStringRef rangeFrom, ConstStringRef rangeTo, OclocArgHelper *argHelper) {
    auto rangeFromStr = rangeFrom.str();
    auto rangeToStr = rangeTo.str();
    ProductConfigHelper::adjustDeviceName(rangeFromStr);
    ProductConfigHelper::adjustDeviceName(rangeToStr);

    auto familyProducts = getProductsForFamilyClosedRange(rangeFromStr, rangeToStr, argHelper);
    if (!familyProducts.empty()) {
        return familyProducts;
    }
    auto releaseProducts = getProductsForReleaseClosedRange(rangeFromStr, rangeToStr, argHelper);
    if (!releaseProducts.empty()) {
        return releaseProducts;
    }
    auto productProducts = getProductsForProductClosedRange(rangeFromStr, rangeToStr, argHelper);
    if (!productProducts.empty()) {
        return productProducts;
    }

    auto target = rangeFromStr + ":" + rangeToStr;
    argHelper->printf("Failed to parse target : %s.\n", target.c_str());
    return {};
}

auto mergeProducts = [](std::vector<ConstStringRef> &dst, std::vector<ConstStringRef> &&src) {
    for (auto &elem : src) {
        if (std::find(dst.begin(), dst.end(), elem) == dst.end()) {
            dst.push_back(std::move(elem));
        }
    }
};

std::vector<ConstStringRef> getProductsForFamilyOpenRange(const std::string &openRangeStr, OclocArgHelper *argHelper, bool rangeTo) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;

    auto getFamilyConfigWithFallback = [&](const std::string &familyStr, FormerProductConfigHelper::Position pos) {
        auto family = prodHelper.getFamilyFromDeviceName(familyStr);
        AOT::PRODUCT_CONFIG config = AOT::UNKNOWN_ISA;
        if (family != AOT::UNKNOWN_FAMILY) {
            config = prodHelper.getLastProductConfigFromFamilyName(family);
        }
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromFamilyName(familyStr, pos);
        }
        return config;
    };

    std::vector<ConstStringRef> requestedProducts;
    auto family = prodHelper.getFamilyFromDeviceName(openRangeStr);
    if (family != AOT::UNKNOWN_FAMILY) {
        if (rangeTo) {
            auto prodConfigFrom = getFamilyConfigWithFallback(formerProdHelper.getFamilyName(Position::firstItem), Position::firstItem);
            auto prodConfigTo = getFamilyConfigWithFallback(formerProdHelper.getFamilyName(Position::lastItem), Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                mergeProducts(requestedProducts, getProductsForRange(prodConfigFrom, prodConfigTo, argHelper));
            }
            unsigned int familyFrom = AOT::UNKNOWN_FAMILY + 1;
            mergeProducts(requestedProducts, getProductsForTargetRange(static_cast<AOT::FAMILY>(familyFrom), family, argHelper, AOT::FAMILY_MAX));
            return requestedProducts;
        } else {
            unsigned int familyTo = AOT::FAMILY_MAX;
            return getProductsForTargetRange(family, static_cast<AOT::FAMILY>(familyTo), argHelper, AOT::FAMILY_MAX);
        }
    } else if (formerProdHelper.isFamilyName(openRangeStr)) {
        if (rangeTo) {
            auto prodConfigFrom = getFamilyConfigWithFallback(formerProdHelper.getFamilyName(Position::firstItem), Position::firstItem);
            auto prodConfigTo = getFamilyConfigWithFallback(openRangeStr, Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                return getProductsForRange(prodConfigFrom, prodConfigTo, argHelper);
            }
        } else {
            auto prodConfigFrom = getFamilyConfigWithFallback(openRangeStr, Position::firstItem);
            auto prodConfigTo = getFamilyConfigWithFallback(formerProdHelper.getFamilyName(Position::lastItem), Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                mergeProducts(requestedProducts, getProductsForRange(prodConfigFrom, prodConfigTo, argHelper));
            }
            unsigned int familyFrom = AOT::UNKNOWN_FAMILY + 1;
            unsigned int familyTo = AOT::FAMILY_MAX;
            mergeProducts(requestedProducts, getProductsForTargetRange(static_cast<AOT::FAMILY>(familyFrom), static_cast<AOT::FAMILY>(familyTo), argHelper, AOT::FAMILY_MAX));
            return requestedProducts;
        }
    }
    return {};
}

std::vector<ConstStringRef> getProductsForReleaseOpenRange(const std::string &openRangeStr, OclocArgHelper *argHelper, bool rangeTo) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;
    std::vector<ConstStringRef> requestedProducts;

    auto getReleaseConfigWithFallback = [&](const std::string &releaseStr, FormerProductConfigHelper::Position pos) {
        auto release = prodHelper.getReleaseFromDeviceName(releaseStr);
        auto config = prodHelper.getLastProductConfigFromReleaseName(release);
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromReleaseName(releaseStr, pos);
        }
        return config;
    };

    auto release = prodHelper.getReleaseFromDeviceName(openRangeStr);
    if (release != AOT::UNKNOWN_RELEASE) {
        if (rangeTo) {
            auto prodConfigFrom = getReleaseConfigWithFallback(formerProdHelper.getReleaseName(Position::firstItem), Position::firstItem);
            auto prodConfigTo = getReleaseConfigWithFallback(formerProdHelper.getReleaseName(Position::lastItem), Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                mergeProducts(requestedProducts, getProductsForRange(prodConfigFrom, prodConfigTo, argHelper));
            }
            unsigned int releaseFrom = AOT::UNKNOWN_RELEASE + 1;
            mergeProducts(requestedProducts, getProductsForTargetRange(static_cast<AOT::RELEASE>(releaseFrom), release, argHelper, AOT::RELEASE_MAX));
            return requestedProducts;
        } else {
            unsigned int releaseTo = AOT::RELEASE_MAX;
            return getProductsForTargetRange(release, static_cast<AOT::RELEASE>(releaseTo), argHelper, AOT::RELEASE_MAX);
        }
    } else if (formerProdHelper.isReleaseName(openRangeStr)) {
        if (rangeTo) {
            auto prodConfigFrom = getReleaseConfigWithFallback(formerProdHelper.getReleaseName(Position::firstItem), Position::firstItem);
            auto prodConfigTo = getReleaseConfigWithFallback(openRangeStr, Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                return getProductsForRange(prodConfigFrom, prodConfigTo, argHelper);
            }
        } else {
            auto prodConfigFrom = getReleaseConfigWithFallback(openRangeStr, Position::firstItem);
            auto prodConfigTo = getReleaseConfigWithFallback(formerProdHelper.getReleaseName(Position::lastItem), Position::lastItem);
            if (prodConfigFrom != AOT::UNKNOWN_ISA && prodConfigTo != AOT::UNKNOWN_ISA) {
                mergeProducts(requestedProducts, getProductsForRange(prodConfigFrom, prodConfigTo, argHelper));
            }
            unsigned int releaseFrom = AOT::UNKNOWN_RELEASE + 1;
            unsigned int releaseTo = AOT::RELEASE_MAX;
            mergeProducts(requestedProducts, getProductsForTargetRange(static_cast<AOT::RELEASE>(releaseFrom), static_cast<AOT::RELEASE>(releaseTo), argHelper, AOT::RELEASE_MAX));
            return requestedProducts;
        }
    }
    return {};
}

std::vector<ConstStringRef> getProductsForProductOpenRange(const std::string &openRangeStr, OclocArgHelper *argHelper, bool rangeTo) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;

    auto getProductConfigWithFallback = [&](const std::string &deviceName) {
        auto config = prodHelper.getProductConfigFromDeviceName(deviceName);
        if (config == AOT::UNKNOWN_ISA) {
            config = formerProdHelper.getProductConfigFromDeviceName(deviceName);
        }
        return config;
    };

    auto product = getProductConfigWithFallback(openRangeStr);
    if (product != AOT::UNKNOWN_ISA) {
        if (rangeTo) {
            unsigned int productFrom = formerProdHelper.getFirstProductConfig();
            if (productFrom == AOT::UNKNOWN_ISA) {
                ++productFrom;
            }
            return getProductsForRange(productFrom, static_cast<unsigned int>(product), argHelper);
        } else {
            unsigned int productTo = AOT::getConfixMaxPlatform() - 1;
            return getProductsForRange(product, static_cast<AOT::PRODUCT_CONFIG>(productTo), argHelper);
        }
    }
    return {};
}

std::vector<ConstStringRef> getProductForOpenRange(ConstStringRef openRange, OclocArgHelper *argHelper, bool rangeTo) {
    auto openRangeStr = openRange.str();
    ProductConfigHelper::adjustDeviceName(openRangeStr);

    auto familyProducts = getProductsForFamilyOpenRange(openRangeStr, argHelper, rangeTo);
    if (!familyProducts.empty()) {
        return familyProducts;
    }
    auto releaseProducts = getProductsForReleaseOpenRange(openRangeStr, argHelper, rangeTo);
    if (!releaseProducts.empty()) {
        return releaseProducts;
    }
    auto productProducts = getProductsForProductOpenRange(openRangeStr, argHelper, rangeTo);
    if (!productProducts.empty()) {
        return productProducts;
    }

    argHelper->printf("Failed to parse target : %s.\n", openRangeStr.c_str());
    return {};
}

std::vector<ConstStringRef> getProductForSpecificTarget(const CompilerOptions::TokenizedString &targets, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;
    std::vector<ConstStringRef> requestedConfigs;

    auto getProductsAcronymsForFamilyWithFallback = [&](const std::string &targetStr) -> bool {
        auto family = prodHelper.getFamilyFromDeviceName(targetStr);
        if (family != AOT::UNKNOWN_FAMILY) {
            getProductsAcronymsForTarget(requestedConfigs, family, argHelper);
            return true;
        }
        if (formerProdHelper.isSupportedTarget<AOT::FAMILY>(targetStr)) {
            mergeProducts(requestedConfigs, formerProdHelper.getProductAcronymsFromFamilyGroup(targetStr));
            return true;
        }
        return false;
    };

    auto getProductsAcronymsForReleaseWithFallback = [&](const std::string &targetStr) -> bool {
        auto release = prodHelper.getReleaseFromDeviceName(targetStr);
        if (release != AOT::UNKNOWN_RELEASE) {
            getProductsAcronymsForTarget(requestedConfigs, release, argHelper);
            return true;
        }
        if (formerProdHelper.isSupportedTarget<AOT::RELEASE>(targetStr)) {
            mergeProducts(requestedConfigs, formerProdHelper.getProductAcronymsFromReleaseGroup(targetStr));
            return true;
        }
        return false;
    };

    auto getProductConfigWithFallback = [&](const ConstStringRef &target) -> bool {
        auto product = prodHelper.getProductConfigFromAcronym(target.str());
        if (product == AOT::UNKNOWN_ISA) {
            product = formerProdHelper.getProductConfigFromDeviceName(target.str());
        }
        if (product != AOT::UNKNOWN_ISA) {
            mergeProducts(requestedConfigs, std::vector<ConstStringRef>{target});
            return true;
        }
        return false;
    };

    for (const auto &target : targets) {
        auto targetStr = target.str();
        ProductConfigHelper::adjustDeviceName(targetStr);

        if (getProductsAcronymsForFamilyWithFallback(targetStr)) {
            continue;
        }
        if (getProductsAcronymsForReleaseWithFallback(targetStr)) {
            continue;
        }
        if (getProductConfigWithFallback(target)) {
            continue;
        }
        if (getHwInfoForDeprecatedAcronym(targetStr) != nullptr) {
            requestedConfigs.push_back(target);
            continue;
        }

        argHelper->printf("Failed to parse target : %s - invalid device:\n", targetStr.c_str());
        return {};
    }

    return requestedConfigs;
}

std::vector<ConstStringRef> getTargetProductsForFatbinary(ConstStringRef deviceArg, OclocArgHelper *argHelper) {
    auto &prodHelper = *argHelper->productConfigHelper;
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;
    std::vector<ConstStringRef> retVal;
    if (deviceArg == "*") {
        retVal = prodHelper.getRepresentativeProductAcronyms();
        auto formerAcronyms = formerProdHelper.getRepresentativeProductAcronyms();
        retVal.insert(retVal.begin(), formerAcronyms.begin(), formerAcronyms.end());
        return retVal;
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

int getDeviceArgValueIdx(const std::vector<std::string> &args) {
    for (size_t argIndex = 0; argIndex < args.size(); ++argIndex) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        if ((ConstStringRef("-device") == currArg) && hasMoreArgs) {
            return static_cast<int>(argIndex + 1);
        }
    }
    return -1;
}

int buildFatBinaryForTarget(int retVal, const std::vector<std::string> &argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                            OfflineCompiler *pCompiler, OclocArgHelper *argHelper, const std::string &product) {
    auto &prodHelper = *argHelper->productConfigHelper;

    if (retVal == 0) {
        retVal = buildWithSafetyGuard(pCompiler);
        std::string buildLog = pCompiler->getBuildLog();
        if (buildLog.empty() == false) {
            argHelper->printf("%s\n", buildLog.c_str());
        }
        if (retVal == 0) {
            if (!pCompiler->isQuiet()) {
                argHelper->printf("Build succeeded for : %s.\n", product.c_str());
            }
        } else {
            argHelper->printf("Build failed for : %s with error code: %d\n", product.c_str(), retVal);
            argHelper->printf("Command was:");
            for (const auto &arg : argsCopy) {
                argHelper->printf(" %s", arg.c_str());
            }
            argHelper->printf("\n");
        }
    }
    if (retVal) {
        return retVal;
    }

    std::string entryName("");
    if (product.find(".") != std::string::npos) {
        entryName = product;
    } else {
        auto productConfig = prodHelper.getProductConfigFromDeviceName(product);
        auto genericIdAcronymIt = std::find_if(AOT::genericIdAcronyms.begin(), AOT::genericIdAcronyms.end(),
                                               [product](const std::pair<std::string, AOT::PRODUCT_CONFIG> &genericIdAcronym) {
                                                   return product == genericIdAcronym.first;
                                               });
        if (AOT::UNKNOWN_ISA != productConfig && genericIdAcronymIt == AOT::genericIdAcronyms.end()) {
            entryName = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
        } else {
            entryName = product;
        }
    }

    fatbinary.appendFileEntry(pointerSize + "." + entryName, pCompiler->getPackedDeviceBinaryOutput());
    return retVal;
}

int buildFatBinaryForFormerTarget(int retVal, const std::vector<std::string> &argsCopy, std::string pointerSize, Ar::ArEncoder &fatbinary,
                                  OclocArgHelper *argHelper, const std::string &product) {
    auto &formerProdHelper = *argHelper->formerProductConfigHelper;
    uint32_t numOutputs = 0u;
    unsigned char **dataOutputs = nullptr;
    uint64_t *lenOutputs = nullptr;
    char **nameOutputs = nullptr;

    if (retVal == 0) {
        std::vector<const char *> argvPtrs;
        argvPtrs.reserve(argsCopy.size());
        for (const auto &arg : argsCopy) {
            argvPtrs.push_back(arg.c_str());
        }
        auto retValFormerOcloc = Ocloc::Commands::invokeFormerOcloc(Ocloc::getOclocFormerLibName(),
                                                                    static_cast<unsigned int>(argvPtrs.size()),
                                                                    argvPtrs.data(),
                                                                    0, nullptr, nullptr, nullptr,
                                                                    0, nullptr, nullptr, nullptr,
                                                                    &numOutputs, &dataOutputs,
                                                                    &lenOutputs, &nameOutputs);
        if (retValFormerOcloc) {
            retVal = retValFormerOcloc.value();
            argHelper->dontSetupOutputs();
            // Check if the actual return code indicates success
            if (retVal == 0) {
                argHelper->printf("Build succeeded for : %s.\n", product.c_str());
            } else {
                argHelper->printf("Build failed for : %s with error code: %d\n", product.c_str(), retVal);
                argHelper->printf("Command was:");
                for (const auto &arg : argsCopy) {
                    argHelper->printf(" %s", arg.c_str());
                }
                argHelper->printf("\n");
                return retVal;
            }
        } else {
            // Former ocloc couldn't be invoked at all
            argHelper->printf("Build failed for : %s - could not invoke former ocloc\n", product.c_str());
            return retVal;
        }

        for (size_t i = 0; i < numOutputs; ++i) {
            std::string name = nameOutputs[i];
            if (name.find(".bin") != std::string::npos) {
                const ArrayRef<const uint8_t> fileData(dataOutputs[i], static_cast<size_t>(lenOutputs[i]));

                std::string entryName("");
                if (product.find(".") != std::string::npos) {
                    entryName = product;
                } else {
                    auto productConfig = formerProdHelper.getProductConfigFromAcronym(product);
                    entryName = ProductConfigHelper::parseMajorMinorRevisionValue(productConfig);
                }
                fatbinary.appendFileEntry(pointerSize + "." + entryName, fileData);
            }
        }
    }

    // Use formerOclocFree since memory was allocated by former ocloc
    auto freeResult = Ocloc::Commands::formerOclocFree(Ocloc::getOclocFormerLibName(), &numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    if (!freeResult) {
        // Fallback to regular oclocFreeOutput if formerOclocFree fails
        oclocFreeOutput(&numOutputs, &dataOutputs, &lenOutputs, &nameOutputs);
    }
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
    std::set<std::string> deviceAcronymsFromDeviceOptions;

    std::vector<std::string> argsCopy(args);
    for (size_t argIndex = 1; argIndex < args.size(); argIndex++) {
        const auto &currArg = args[argIndex];
        const bool hasMoreArgs = (argIndex + 1 < args.size());
        const bool hasAtLeast2MoreArgs = (argIndex + 2 < args.size());
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
        } else if (((ConstStringRef("-output") == currArg) || (ConstStringRef("-o") == currArg)) && hasMoreArgs) {
            outputFileName = args[argIndex + 1];
            ++argIndex;
        } else if ((ConstStringRef("-out_dir") == currArg) && hasMoreArgs) {
            outputDirectory = args[argIndex + 1];
            ++argIndex;
        } else if (ConstStringRef("-exclude_ir") == currArg) {
            excludeIr = true;
        } else if (ConstStringRef("-spirv_input") == currArg) {
            spirvInput = true;
        } else if (("-device_options" == currArg) && hasAtLeast2MoreArgs) {
            const auto deviceAcronyms = CompilerOptions::tokenize(args[argIndex + 1], ',');
            for (const auto &deviceAcronym : deviceAcronyms) {
                deviceAcronymsFromDeviceOptions.insert(deviceAcronym.str());
            }
            argIndex += 2;
        }
    }

    const bool shouldPreserveGenericIr = spirvInput && !excludeIr;
    if (shouldPreserveGenericIr) {
        argsCopy.push_back("-exclude_ir");
    }

    if (deviceArgIndex == static_cast<size_t>(-1)) {
        argHelper->printf("Error! Command does not contain device argument!\n");
        return OCLOC_INVALID_COMMAND_LINE;
    }

    Ar::ArEncoder fatbinary(true);
    std::vector<ConstStringRef> targetProducts;
    targetProducts = getTargetProductsForFatbinary(ConstStringRef(args[deviceArgIndex]), argHelper);
    if (targetProducts.empty()) {
        argHelper->printf("Failed to parse target devices from : %s\n", args[deviceArgIndex].c_str());
        return 1;
    }

    for (const auto &deviceAcronym : deviceAcronymsFromDeviceOptions) {
        if (std::find(targetProducts.begin(), targetProducts.end(), deviceAcronym) == targetProducts.end()) {
            argHelper->printf("Warning! -device_options set for non-compiled device: %s\n", deviceAcronym.c_str());
        }
    }

    std::string optionsForIr;
    for (const auto &product : targetProducts) {
        int retVal = OCLOC_SUCCESS;
        argsCopy[deviceArgIndex] = product.str();

        auto &formerProdHelper = *argHelper->formerProductConfigHelper;
        auto formerProduct = formerProdHelper.getProductConfigFromDeviceName(product.str().c_str());
        auto formerProductFallback = formerProdHelper.isSupportedProductConfig(formerProduct);
        if (formerProductFallback) {
            retVal = buildFatBinaryForFormerTarget(retVal, argsCopy, pointerSizeInBits, fatbinary, argHelper, product.str());
            if (retVal) {
                return retVal;
            }
        } else {
            std::unique_ptr<OfflineCompiler> pCompiler{OfflineCompiler::create(argsCopy.size(), argsCopy, false, retVal, argHelper)};
            if (OCLOC_SUCCESS != retVal) {
                argHelper->printf("Error! Couldn't create OfflineCompiler. Exiting.\n");
                return retVal;
            }

            retVal = buildFatBinaryForTarget(retVal, argsCopy, pointerSizeInBits, fatbinary, pCompiler.get(), argHelper, product.str());
            if (retVal) {
                return retVal;
            }
            if (optionsForIr.empty()) {
                optionsForIr = pCompiler->getOptions();
            }
        }
    }

    if (shouldPreserveGenericIr) {
        const auto errorCode = appendGenericIr(fatbinary, inputFileName, argHelper, optionsForIr);
        if (errorCode != OCLOC_SUCCESS) {
            argHelper->printf("Error! Couldn't append generic IR file!\n");
            return errorCode;
        }
    }

    auto fatbinaryData = fatbinary.encode();

    std::string fatbinaryFileName = "";

    if (!outputDirectory.empty() && outputDirectory != "/dev/null") {
        fatbinaryFileName = outputDirectory + "/";
        NEO::Directory(outputDirectory).parseDirectories(Directory::createDirs);
    }

    if (!outputFileName.empty()) {
        fatbinaryFileName += outputFileName;
    } else {
        if (!inputFileName.empty()) {
            fatbinaryFileName += OfflineCompiler::getFileNameTrunk(inputFileName) + ".ar";
        }
    }

    argHelper->saveOutput(fatbinaryFileName, fatbinaryData.data(), fatbinaryData.size());

    return 0;
}

int appendGenericIr(Ar::ArEncoder &fatbinary, const std::string &inputFile, OclocArgHelper *argHelper, std::string options) {
    std::size_t fileSize = 0;
    std::unique_ptr<char[]> fileContents = argHelper->loadDataFromFile(inputFile, fileSize);
    if (fileSize == 0) {
        argHelper->printf("Error! Couldn't read input file!\n");
        return OCLOC_INVALID_FILE;
    }

    const auto ir = ArrayRef<const uint8_t>::fromAny(fileContents.get(), fileSize);
    const auto opt = ArrayRef<const uint8_t>::fromAny(options.data(), options.size());
    if (!isSpirVBitcode(ir)) {
        argHelper->printf("Error! Input file is not in supported generic IR format! "
                          "Currently supported format is SPIR-V.\n");
        return OCLOC_INVALID_FILE;
    }

    const auto encodedElf = createEncodedElfWithSpirv(ir, opt);
    ArrayRef<const uint8_t> genericIrFile{encodedElf.data(), encodedElf.size()};

    fatbinary.appendFileEntry("generic_ir", genericIrFile);
    return OCLOC_SUCCESS;
}

std::vector<uint8_t> createEncodedElfWithSpirv(const ArrayRef<const uint8_t> &spirv, const ArrayRef<const uint8_t> &options) {
    using namespace NEO::Elf;
    ElfEncoder<EI_CLASS_64> elfEncoder;
    elfEncoder.getElfFileHeader().type = ET_OPENCL_OBJECTS;
    elfEncoder.appendSection(SHT_OPENCL_SPIRV, SectionNamesOpenCl::spirvObject, spirv);
    elfEncoder.appendSection(SHT_OPENCL_OPTIONS, SectionNamesOpenCl::buildOptions, options);

    return elfEncoder.encode();
}

} // namespace NEO
