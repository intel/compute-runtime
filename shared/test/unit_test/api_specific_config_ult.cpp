/*
 * Copyright (C) 2023 Intel Corporation
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
bool isStatelessCompressionSupportedForUlts = true;

StackVec<const char *, 4> validUltL0Prefixes = {"NEO_L0_", "NEO_", ""};
StackVec<NEO::DebugVarPrefix, 4> validUltL0PrefixTypes = {DebugVarPrefix::Neo_L0, DebugVarPrefix::Neo, DebugVarPrefix::None};
StackVec<const char *, 4> validUltOclPrefixes = {"NEO_OCL_", "NEO_", ""};
StackVec<NEO::DebugVarPrefix, 4> validUltOclPrefixTypes = {DebugVarPrefix::Neo_Ocl, DebugVarPrefix::Neo, DebugVarPrefix::None};

bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return isStatelessCompressionSupportedForUlts;
}
bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration() {
    return debugManager.flags.UseExternalAllocatorForSshAndDsh.get();
}
bool ApiSpecificConfig::getBindlessMode(const ReleaseHelper *) {
    if (debugManager.flags.UseBindlessMode.get() != -1) {
        return debugManager.flags.UseBindlessMode.get();
    } else {
        return false;
    }
}

bool ApiSpecificConfig::isDeviceAllocationCacheEnabled() {
    return false;
}

bool ApiSpecificConfig::isDynamicPostSyncAllocLayoutEnabled() {
    return (NEO::debugManager.flags.EnableDynamicPostSyncAllocLayout.get() == 1);
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
    if (apiTypeForUlts == ApiSpecificConfig::L0) {
        return validUltL0PrefixTypes;
    } else {
        return validUltOclPrefixTypes;
    }
}

} // namespace NEO
