/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getGmmUsageType(AllocationType allocationType, bool forceUncached, const ProductHelper &productHelper) {
    if (debugManager.flags.ForceUncachedGmmUsageType.get()) {
        UNRECOVERABLE_IF(allocationType == AllocationType::UNKNOWN);
        if ((1llu << (static_cast<int64_t>(allocationType) - 1)) & debugManager.flags.ForceUncachedGmmUsageType.get()) {
            forceUncached = true;
        }
    }

    if (forceUncached || debugManager.flags.ForceAllResourcesUncached.get()) {
        return getDefaultUsageTypeWithCachingDisabled(allocationType);
    } else {
        return getDefaultUsageTypeWithCachingEnabled(allocationType, productHelper);
    }
}

bool CacheSettingsHelper::preferNoCpuAccess(GMM_RESOURCE_USAGE_TYPE_ENUM gmmResourceUsageType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.EnableCpuCacheForResources.get()) {
        return false;
    }
    if (rootDeviceEnvironment.isWddmOnLinux()) {
        return false;
    }
    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    if (!releaseHelper || releaseHelper->isCachingOnCpuAvailable()) {
        return false;
    }
    return (gmmResourceUsageType != GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER);
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType, const ProductHelper &productHelper) {

    switch (allocationType) {
    case AllocationType::IMAGE:
        return GMM_RESOURCE_USAGE_OCL_IMAGE;
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::LINEAR_STREAM:
        if (debugManager.flags.DisableCachingForHeaps.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
    case AllocationType::CONSTANT_SURFACE:
        if (debugManager.flags.ForceL1Caching.get() == 0) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER_CONST;
    case AllocationType::BUFFER:
    case AllocationType::SHARED_BUFFER:
    case AllocationType::SVM_GPU:
    case AllocationType::UNIFIED_SHARED_MEMORY:
    case AllocationType::EXTERNAL_HOST_PTR:
        if (debugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_BUFFER;
    case AllocationType::BUFFER_HOST_MEMORY:
    case AllocationType::INTERNAL_HOST_MEMORY:
    case AllocationType::MAP_ALLOCATION:
    case AllocationType::FILL_PATTERN:
    case AllocationType::SVM_CPU:
    case AllocationType::SVM_ZERO_COPY:
        if (debugManager.flags.DisableCachingForStatefulBufferAccess.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER;
    case AllocationType::GPU_TIMESTAMP_DEVICE_BUFFER:
    case AllocationType::TIMESTAMP_PACKET_TAG_BUFFER:
        if (productHelper.isDcFlushAllowed()) {
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
