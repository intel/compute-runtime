/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

#include <string>
#include <vector>

namespace NEO {
std::vector<const char *> validClPrefixes;
std::vector<NEO::DebugVarPrefix> validClPrefixTypes;
bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return true;
}

bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration() {
    return false;
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
    return false;
}

ApiSpecificConfig::ApiType ApiSpecificConfig::getApiType() {
    return ApiSpecificConfig::OCL;
}

std::string ApiSpecificConfig::getName() {
    return "ocl";
}

uint64_t ApiSpecificConfig::getReducedMaxAllocSize(uint64_t maxAllocSize) {
    return maxAllocSize / 2;
}

const char *ApiSpecificConfig::getRegistryPath() {
    return oclRegPath;
}

void ApiSpecificConfig::initPrefixes() {
    validClPrefixes = {"NEO_OCL_", "NEO_", ""};
    validClPrefixTypes = {DebugVarPrefix::Neo_Ocl, DebugVarPrefix::Neo, DebugVarPrefix::None};
}

const std::vector<const char *> &ApiSpecificConfig::getPrefixStrings() {
    return validClPrefixes;
}

const std::vector<DebugVarPrefix> &ApiSpecificConfig::getPrefixTypes() {
    return validClPrefixTypes;
}
} // namespace NEO
