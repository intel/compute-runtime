/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

#include <string>
#include <vector>

namespace NEO {
class ReleaseHelper;

ApiSpecificConfig::ApiType apiTypeForUlts = ApiSpecificConfig::OCL;

bool globalStatelessL0 = false;
bool globalStatelessOcl = false;
bool isStatelessCompressionSupportedForUlts = true;
bool isDeviceUsmPoolingEnabledForUlts = true;

const StackVec<DebugVarPrefix, 4> *validUltPrefixTypesOverride = nullptr;
StackVec<const char *, 4> validUltL0Prefixes = {"NEO_L0_", "NEO_", ""};
StackVec<NEO::DebugVarPrefix, 4> validUltL0PrefixTypes = {DebugVarPrefix::neoL0, DebugVarPrefix::neo, DebugVarPrefix::none};
StackVec<const char *, 4> validUltOclPrefixes = {"NEO_OCL_", "NEO_", ""};
StackVec<NEO::DebugVarPrefix, 4> validUltOclPrefixTypes = {DebugVarPrefix::neoOcl, DebugVarPrefix::neo, DebugVarPrefix::none};

bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return isStatelessCompressionSupportedForUlts;
}
bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration(const ReleaseHelper *releaseHelper) {
    if (debugManager.flags.UseExternalAllocatorForSshAndDsh.get() != -1) {
        return debugManager.flags.UseExternalAllocatorForSshAndDsh.get();
    }
    return false;
}
bool ApiSpecificConfig::getBindlessMode(const Device &device) {
    if (debugManager.flags.UseBindlessMode.get() != -1) {
        return debugManager.flags.UseBindlessMode.get();
    } else {
        return false;
    }
}

bool ApiSpecificConfig::isDeviceAllocationCacheEnabled() {
    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return false;
    } else {
        return true;
    }
}

bool ApiSpecificConfig::isHostAllocationCacheEnabled() {
    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return false;
    } else {
        return true;
    }
}

bool ApiSpecificConfig::isDeviceUsmPoolingEnabled() {
    return isDeviceUsmPoolingEnabledForUlts;
}

bool ApiSpecificConfig::isHostUsmPoolingEnabled() {
    return false;
}

ApiSpecificConfig::ApiType ApiSpecificConfig::getApiType() {
    return apiTypeForUlts;
}

uint64_t ApiSpecificConfig::getReducedMaxAllocSize(uint64_t maxAllocSize) {
    return maxAllocSize / 2;
}

std::string ApiSpecificConfig::getName() {
    return "shared";
}
const char *ApiSpecificConfig::getRegistryPath() {
    return "";
}

void ApiSpecificConfig::initPrefixes() {
}

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return validUltL0Prefixes;
    } else {
        return validUltOclPrefixes;
    }
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    if (validUltPrefixTypesOverride) {
        return *validUltPrefixTypesOverride;
    }
    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return validUltL0PrefixTypes;
    } else {
        return validUltOclPrefixTypes;
    }
}

std::string ApiSpecificConfig::compilerCacheDir() {
    return "cl_cache_dir";
}

std::string ApiSpecificConfig::compilerCacheLocation() {
    return "cl_cache";
}

std::string ApiSpecificConfig::compilerCacheFileExtension() {
    return ".cl_cache";
}

bool ApiSpecificConfig::isUpdateTagFromWaitEnabledForHeapless() {
    return true;
}

int64_t ApiSpecificConfig::compilerCacheDefaultEnabled() {
    return 1l;
}

bool ApiSpecificConfig::isGlobalStatelessEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) {

    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return globalStatelessL0;

    } else {
        return globalStatelessOcl;
    }
}

} // namespace NEO
