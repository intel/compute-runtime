/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
#include "shared/source/helpers/cache_policy_option_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"

#include <cstdint>
#include <cstring>
#include <sstream>

namespace NEO {

const std::string clStdOptionName = "-cl-std=CL";

std::pair<uint32_t, uint32_t> getMajorMinorVersion(const std::string &compileOptions) {
    const std::string clStdOptionName = "-cl-std=CL";
    auto clStdValuePosition = compileOptions.find(clStdOptionName);
    if (clStdValuePosition == std::string::npos) {
        return {0, 0};
    }
    std::stringstream ss{compileOptions.c_str() + clStdValuePosition + clStdOptionName.size()};
    uint32_t majorVersion = 0, minorVersion = 0;
    char dot = 0;
    ss >> majorVersion >> dot >> minorVersion;
    return {majorVersion, minorVersion};
}

bool requiresOpenClCFeatures(const std::string &compileOptions) {
    return (getMajorMinorVersion(compileOptions).first >= 3);
}

bool requiresAdditionalExtensions(const std::string &compileOptions) {
    return (getMajorMinorVersion(compileOptions).first == 2);
}

bool isOclVersionBelow12(const std::string &compileOptions) {
    auto [majorVersion, minorVersion] = getMajorMinorVersion(compileOptions);
    return majorVersion == 1 && minorVersion < 2;
}

bool replaceL1CachePolicyInBuildOptions(std::string &buildOptions, const char *currentCachePolicy) {
    if (currentCachePolicy) {
        auto currentCachePolicyIter = buildOptions.find(currentCachePolicy);
        if (currentCachePolicyIter == std::string::npos) {
            CachePolicyOptionHelper::replaceCachePolicy(buildOptions, currentCachePolicy);
            return true;
        }
    }
    return false;
}

bool checkAndReplaceL1CachePolicy(std::string &buildOptions, NEO::Zebin::ZeInfo::Types::Version version, const char *currentCachePolicy) {
    if (!requiresL1PolicyMissmatchCheck()) {
        return false;
    }

    // After 1.60 all binaries should have WB cache policy
    if (version.major >= 1 && version.minor > 60) {
        return false;
    }

    // No ze info in binary
    if (version.major == 0 && version.minor == 0) {
        return false;
    }

    return replaceL1CachePolicyInBuildOptions(buildOptions, currentCachePolicy);
}

bool checkL1CachePolicyMismatch(NEO::Zebin::ZeInfo::Types::L1CachePolicy::L1CachePolicy binaryPolicy, uint32_t driverDefaultL1CacheControl) {
    using L1CachePolicy = NEO::Zebin::ZeInfo::Types::L1CachePolicy::L1CachePolicy;
    if (!requiresL1PolicyMissmatchCheck()) {
        return false;
    }

    // The zeinfo l1_cache_policy is optional; when the binary does not declare one there is
    // nothing to compare here - fall back to the build-options based check.
    if (binaryPolicy == L1CachePolicy::L1CachePolicyUnknown) {
        return false;
    }

    // Map the zebin policy enum onto encoding (WBP=0, UC=1, WB=2, WT=3, WS=4)
    // so it can be compared with the driver default returned by ProductHelper::getL1CachePolicy().
    uint32_t binaryL1CacheControl = 0;
    switch (binaryPolicy) {
    case L1CachePolicy::L1CachePolicyWriteBypass:
        binaryL1CacheControl = 0; // L1_CACHE_CONTROL_WBP
        break;
    case L1CachePolicy::L1CachePolicyUncached:
        binaryL1CacheControl = 1; // L1_CACHE_CONTROL_UC
        break;
    case L1CachePolicy::L1CachePolicyWriteBack:
        binaryL1CacheControl = 2; // L1_CACHE_CONTROL_WB
        break;
    case L1CachePolicy::L1CachePolicyWriteThrough:
        binaryL1CacheControl = 3; // L1_CACHE_CONTROL_WT
        break;
    case L1CachePolicy::L1CachePolicyWriteStreaming:
        binaryL1CacheControl = 4; // L1_CACHE_CONTROL_WS
        break;
    default:
        return false;
    }

    return binaryL1CacheControl != driverDefaultL1CacheControl;
}

void appendAdditionalExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions) {
    if (requiresAdditionalExtensions(compileOptions)) {
        extensions.erase(extensions.length() - 1);
        extensions += ",+cl_khr_3d_image_writes ";
    }
    if (std::string::npos != internalOptions.find("-cl-fp64-gen-emu")) {
        extensions.erase(extensions.length() - 1);
        extensions += ",+__opencl_c_fp64,+cl_khr_fp64 ";
    }
}

void removeMsaaSharingExtension(std::string &extensions) {
    const std::string toRemove = "+cl_khr_gl_msaa_sharing";
    size_t pos = extensions.find(toRemove);
    if (pos != std::string::npos) {
        extensions.erase(pos, toRemove.length());
    }
}

void removeNotSupportedExtensions(std::string &extensions, const std::string &compileOptions, const std::string &internalOptions) {
    if (isOclVersionBelow12(compileOptions)) {
        removeMsaaSharingExtension(extensions);
    }
}

void appendExtensionsToInternalOptions(const HardwareInfo &hwInfo, const std::string &options, std::string &internalOptions) {
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    UNRECOVERABLE_IF(!compilerProductHelper);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, *releaseHelper);

    if (requiresAdditionalExtensions(options)) {
        extensionsList += "cl_khr_3d_image_writes ";
    }
    if (hwInfo.capabilityTable.supportsImages) {
        extensionsList += "cl_khr_gl_msaa_sharing ";
        extensionsList += "cl_khr_gl_depth_images ";
    }
    OpenClCFeaturesContainer openclCFeatures;
    if (requiresOpenClCFeatures(options)) {
        getOpenclCFeaturesList(hwInfo, openclCFeatures, *compilerProductHelper.get(), *releaseHelper);
    }

    auto compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), openclCFeatures);
    auto oclVersion = std::string(oclVersionCompilerInternalOption);
    internalOptions = CompilerOptions::concatenate(oclVersion, compilerExtensions, internalOptions);
    if (hwInfo.capabilityTable.supportsImages) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableImageSupport);
    }
}

} // namespace NEO
