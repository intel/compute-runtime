/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"

#include "opencl/source/os_interface/ocl_reg_path.h"

#include <string>

namespace NEO {

bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration(const HardwareInfo &hwInfo) {
    return false;
}

bool ApiSpecificConfig::getBindlessMode(const Device &device) {
    if (device.getCompilerProductHelper().isHeaplessModeEnabled(device.getHardwareInfo())) {
        return true;
    }

    if (debugManager.flags.UseBindlessMode.get() != -1) {
        return debugManager.flags.UseBindlessMode.get();
    } else {
        return false;
    }
}

bool ApiSpecificConfig::isDeviceAllocationCacheEnabled() {
    return true;
}

bool ApiSpecificConfig::isHostAllocationCacheEnabled() {
    return true;
}

bool ApiSpecificConfig::isDeviceUsmPoolingEnabled() {
    return true;
}

bool ApiSpecificConfig::isHostUsmPoolingEnabled() {
    return true;
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

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    static const StackVec<const char *, 4> validClPrefixes = {"NEO_OCL_", "NEO_", ""};
    return validClPrefixes;
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    static const StackVec<DebugVarPrefix, 4> validClPrefixTypes = {DebugVarPrefix::neoOcl, DebugVarPrefix::neo, DebugVarPrefix::none};
    return validClPrefixTypes;
}

std::string ApiSpecificConfig::compilerCacheFileExtension() {
    return ".cl_cache";
}

int64_t ApiSpecificConfig::compilerCacheDefaultEnabled() {
    return 1l;
}

bool ApiSpecificConfig::isGlobalStatelessEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

bool ApiSpecificConfig::isUpdateTagFromWaitEnabledForHeapless() {
    return true;
}

} // namespace NEO
