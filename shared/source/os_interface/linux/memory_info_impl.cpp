/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

#include <cstddef>
#include <vector>

namespace NEO {

MemoryInfoImpl::MemoryInfoImpl(const drm_i915_memory_region_info *regionInfo, size_t count)
    : drmQueryRegions(regionInfo, regionInfo + count), systemMemoryRegion(drmQueryRegions[0]) {
    UNRECOVERABLE_IF(systemMemoryRegion.region.memory_class != I915_MEMORY_CLASS_SYSTEM);
    std::copy_if(drmQueryRegions.begin(), drmQueryRegions.end(), std::back_inserter(localMemoryRegions),
                 [](const drm_i915_memory_region_info &memoryRegionInfo) {
                     return (memoryRegionInfo.region.memory_class == I915_MEMORY_CLASS_DEVICE);
                 });
}

void MemoryInfoImpl::assignRegionsFromDistances(const void *distanceInfosPtr, size_t size) {
}

} // namespace NEO
