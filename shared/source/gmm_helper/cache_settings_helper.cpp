/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getGmmUsageType(AllocationType allocationType, bool forceUncached, const HardwareInfo &hwInfo) {
    if (forceUncached || DebugManager.flags.ForceAllResourcesUncached.get()) {
        return getDefaultUsageTypeWithCachingDisabled(allocationType);
    } else {
        return getDefaultUsageTypeWithCachingEnabled(allocationType, hwInfo);
    }
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const HardwareInfo &hwInfo) {
    const auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    switch (allocationType) {
    case AllocationType::IMAGE:
        return GMM_RESOURCE_USAGE_OCL_IMAGE;
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::LINEAR_STREAM:
        if (DebugManager.flags.DisableCachingForHeaps.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
    case AllocationType::CONSTANT_SURFACE:
        if (DebugManager.flags.ForceL1Caching.get() == 0) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
    case AllocationType::BUFFER:
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::EXTERNAL_HOST_PTR:
    case AllocationType::FILL_PATTERN:
    case AllocationType::INTERNAL_HOST_MEMORY:
    case AllocationType::MAP_ALLOCATION:
    case AllocationType::SHARED_BUFFER:
    case AllocationType::SVM_CPU:
    case AllocationType::SVM_GPU:
    case AllocationType::SVM_ZERO_COPY:
    case AllocationType::UNIFIED_SHARED_MEMORY:
        if (DebugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        if (hwInfoConfig->isDcFlushAllowed()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    default:
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    }
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingDisabled(AllocationType allocationType) {
    switch (allocationType) {
    case AllocationType::PREEMPTION:
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC;
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::LINEAR_STREAM:
        return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED;
    default:
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }
}

} // namespace NEO