/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_options_parser.h"

#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/compiler_interface/oclc_extensions.h"
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

    if (currentCachePolicy) {
        auto currentCachePolicyIter = buildOptions.find(currentCachePolicy);
        if (currentCachePolicyIter == std::string::npos) {
            constexpr const char *cachePolicyPrefix = "-cl-store-cache-default=";

            auto cachePolicyIter = buildOptions.find(cachePolicyPrefix);
            if (cachePolicyIter != std::string::npos) {
                buildOptions.replace(cachePolicyIter, strlen(currentCachePolicy), currentCachePolicy);
            }
            return true;
        }
    }
    return false;
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
    std::string extensionsList = compilerProductHelper->getDeviceExtensions(hwInfo, releaseHelper.get());

    if (requiresAdditionalExtensions(options)) {
        extensionsList += "cl_khr_3d_image_writes ";
    }
    OpenClCFeaturesContainer openclCFeatures;
    if (requiresOpenClCFeatures(options)) {
        getOpenclCFeaturesList(hwInfo, openclCFeatures, *compilerProductHelper.get(), releaseHelper.get());
    }

    auto compilerExtensions = convertEnabledExtensionsToCompilerInternalOptions(extensionsList.c_str(), openclCFeatures);
    auto oclVersion = std::string(oclVersionCompilerInternalOption);
    internalOptions = CompilerOptions::concatenate(oclVersion, compilerExtensions, internalOptions);
    if (hwInfo.capabilityTable.supportsImages) {
        CompilerOptions::concatenateAppend(internalOptions, CompilerOptions::enableImageSupport);
    }
}

} // namespace NEO
