/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getGmmUsageType(AllocationType allocationType, bool forceUncached) {
    if (forceUncached) {
        return getDefaultUsageTypeWithCachingDisabled(allocationType);
    } else {
        return getDefaultUsageTypeWithCachingEnabled(allocationType);
    }
}

GMM_RESOURCE_USAGE_TYPE_ENUM CacheSettingsHelper::getDefaultUsageTypeWithCachingEnabled(AllocationType allocationType) {
    switch (allocationType) {
    case AllocationType::IMAGE:
        return GMM_RESOURCE_USAGE_OCL_IMAGE;
    case AllocationType::INTERNAL_HEAP:
    case AllocationType::LINEAR_STREAM:
        if (DebugManager.flags.DisableCachingForHeaps.get()) {
            return getDefaultUsageTypeWithCachingDisabled(allocationType);
        }
        return GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER;
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