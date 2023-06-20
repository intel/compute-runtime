/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/compiler_interface/default_cache_config.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

namespace NEO {
namespace ImplicitScaling {
bool apiSupport = false;
} // namespace ImplicitScaling
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    return false;
}
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
}
void PageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
}
CompilerCacheConfig getDefaultCompilerCacheConfig() { return {}; }
const char *getAdditionalBuiltinAsString(EBuiltInOps::Type builtin) { return nullptr; }

void RootDeviceEnvironment::initApiGfxCoreHelper() {
}

ApiSpecificConfig::ApiType apiTypeForUlts = ApiSpecificConfig::OCL;
bool isStatelessCompressionSupportedForUlts = true;

bool ApiSpecificConfig::isStatelessCompressionSupported() {
    return isStatelessCompressionSupportedForUlts;
}
bool ApiSpecificConfig::isBcsSplitWaSupported() {
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
    return (NEO::DebugManager.flags.EnableDynamicPostSyncAllocLayout.get() == 1);
}

bool ApiSpecificConfig::isRelaxedOrderingEnabled() {
    return true;
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

} // namespace NEO
