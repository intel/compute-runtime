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
StackVec<const char *, 4> validClPrefixes;
StackVec<NEO::DebugVarPrefix, 4> validClPrefixTypes;
bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return true;
}

bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration() {
    return false;
}

bool ApiSpecificConfig::getBindlessMode(const ReleaseHelper *releaseHelper) {
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
    validClPrefixTypes = {DebugVarPrefix::neoOcl, DebugVarPrefix::neo, DebugVarPrefix::none};
}

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    return validClPrefixes;
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    return validClPrefixTypes;
}
} // namespace NEO
