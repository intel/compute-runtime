/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/ail/ail_configuration.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include "level_zero/core/source/compiler_interface/l0_reg_path.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include <string>
#include <vector>

namespace NEO {
StackVec<const char *, 4> validL0Prefixes;
StackVec<NEO::DebugVarPrefix, 4> validL0PrefixTypes;
bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return false;
}

bool ApiSpecificConfig::getGlobalBindlessHeapConfiguration(const ReleaseHelper *releaseHelper) {
    if (debugManager.flags.UseExternalAllocatorForSshAndDsh.get() != -1) {
        return debugManager.flags.UseExternalAllocatorForSshAndDsh.get();
    }
    return releaseHelper ? releaseHelper->isGlobalBindlessAllocatorEnabled() : false;
}

bool ApiSpecificConfig::getBindlessMode(const Device &device) {
    if (device.getCompilerProductHelper().isForceBindlessRequired(device.getHardwareInfo())) {
        return true;
    }

    if (debugManager.flags.UseBindlessMode.get() != -1) {
        return debugManager.flags.UseBindlessMode.get();
    }

    auto ailHelper = device.getAilConfigurationHelper();
    auto releaseHelper = device.getReleaseHelper();
    if (ailHelper && ailHelper->disableBindlessAddressing()) {
        return false;
    } else {
        return releaseHelper ? !releaseHelper->isBindlessAddressingDisabled() : false;
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
    validL0Prefixes = {"NEO_L0_", "NEO_", ""};
    validL0PrefixTypes = {DebugVarPrefix::neoL0, DebugVarPrefix::neo, DebugVarPrefix::none};
}

const StackVec<const char *, 4> &ApiSpecificConfig::getPrefixStrings() {
    return validL0Prefixes;
}

const StackVec<DebugVarPrefix, 4> &ApiSpecificConfig::getPrefixTypes() {
    return validL0PrefixTypes;
}

bool ApiSpecificConfig::isSharedAllocPrefetchEnabled() {
    return (NEO::debugManager.flags.ForceMemoryPrefetchForKmdMigratedSharedAllocations.get() ||
            (NEO::debugManager.flags.EnableBOChunkingPrefetch.get() && ((NEO::debugManager.flags.EnableBOChunking.get()) != -1) && ((NEO::debugManager.flags.EnableBOChunking.get()) & 0x1)));
}

std::string ApiSpecificConfig::compilerCacheFileExtension() {
    return ".l0_cache";
}

int64_t ApiSpecificConfig::compilerCacheDefaultEnabled() {
    return 1l;
}

bool ApiSpecificConfig::isGlobalStatelessEnabled(const RootDeviceEnvironment &rootDeviceEnvironment) {

    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0::L0GfxCoreHelper>();
    return l0GfxCoreHelper.getHeapAddressModel(rootDeviceEnvironment) == HeapAddressModel::globalStateless;
}

bool ApiSpecificConfig::isUpdateTagFromWaitEnabledForHeapless() {
    return false;
}

} // namespace NEO
