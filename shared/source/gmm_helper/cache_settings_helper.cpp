/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/cache_settings_helper.h"

#include "shared/source/memory_manager/allocation_type.h"

namespace NEO {

namespace CacheSettingsHelper {
GMM_RESOURCE_USAGE_TYPE_ENUM getGmmUsageType(AllocationType allocationType, bool forceUncached) {
    if (forceUncached) {
        return (allocationType == AllocationType::PREEMPTION) ? GMM_RESOURCE_USAGE_OCL_BUFFER_CSR_UC
                                                              : GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }

    if (allocationType == AllocationType::IMAGE) {
        return GMM_RESOURCE_USAGE_OCL_IMAGE;
    }

    return GMM_RESOURCE_USAGE_OCL_BUFFER;
}
} // namespace CacheSettingsHelper
} // namespace NEO