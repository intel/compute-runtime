/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"

#include <string>
#include <vector>

namespace NEO {
std::vector<const char *> validL0Prefixes;
std::vector<NEO::DebugVarPrefix> validL0PrefixTypes;
bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return false;
}

bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration() {
    return DebugManager.flags.UseExternalAllocatorForSshAndDsh.get();
}

bool ApiSpecificConfig::getBindlessMode() {
    if (DebugManager.flags.UseBindlessMode.get() != -1) {
        return DebugManager.flags.UseBindlessMode.get();
    } else {
        return false;
    }
}

bool ApiSpecificConfig::isDeviceAllocationCacheEnabled() {
    return false;
}

bool ApiSpecificConfig::isDynamicPostSyncAllocLayoutEnabled() {
    return (NEO::DebugManager.flags.EnableDynamicPostSyncAllocLayout.get() != 0);
}

ApiSpecificConfig::ApiType ApiSpecificConfig::getApiType() {
    return ApiSpecificConfig::L0;
}

std::string ApiSpecificConfig::getName() {
    return "l0";
}

uint64_t ApiSpecificConfig::getReducedMaxAllocSize(uint64_t maxAllocSize) {
    return maxAllocSize;
}

const char *ApiSpecificConfig::getRegistryPath() {
    return L0::registryPath;
}

void ApiSpecificConfig::initPrefixes() {
    validL0Prefixes.push_back("NEO_L0_");
    validL0Prefixes.push_back("NEO_");
    validL0Prefixes.push_back("");
    validL0PrefixTypes.push_back(DebugVarPrefix::Neo_L0);
    validL0PrefixTypes.push_back(DebugVarPrefix::Neo);
    validL0PrefixTypes.push_back(DebugVarPrefix::None);
}

const std::vector<const char *> &ApiSpecificConfig::getPrefixStrings() {
    return validL0Prefixes;
}

const std::vector<DebugVarPrefix> &ApiSpecificConfig::getPrefixTypes() {
    return validL0PrefixTypes;
}
} // namespace NEO
