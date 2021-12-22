/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/memory_info.h"

#include "drm/i915_drm.h"

#include <algorithm>
#include <iterator>

namespace NEO {

MemoryInfo::MemoryInfo(const RegionContainer &regionInfo)
    : drmQueryRegions(regionInfo), systemMemoryRegion(drmQueryRegions[0]) {
    UNRECOVERABLE_IF(systemMemoryRegion.region.memoryClass != I915_MEMORY_CLASS_SYSTEM);
    std::copy_if(drmQueryRegions.begin(), drmQueryRegions.end(), std::back_inserter(localMemoryRegions),
                 [](const MemoryRegion &memoryRegionInfo) {
                     return (memoryRegionInfo.region.memoryClass == I915_MEMORY_CLASS_DEVICE);
                 });
}

void MemoryInfo::assignRegionsFromDistances(const void *distanceInfosPtr, size_t size) {
}

} // namespace NEO
