/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info_impl.h"

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "drm/i915_drm.h"

namespace NEO {

MemoryInfoImpl::MemoryInfoImpl(const drm_i915_memory_region_info *regionInfo, size_t count)
    : drmQueryRegions(regionInfo, regionInfo + count), systemMemoryRegion(drmQueryRegions[0]) {
    UNRECOVERABLE_IF(systemMemoryRegion.region.memory_class != I915_MEMORY_CLASS_SYSTEM);
    std::copy_if(drmQueryRegions.begin(), drmQueryRegions.end(), std::back_inserter(localMemoryRegions),
                 [](const drm_i915_memory_region_info &memoryRegionInfo) {
                     return (memoryRegionInfo.region.memory_class == I915_MEMORY_CLASS_DEVICE);
                 });
}

uint32_t MemoryInfoImpl::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    auto pHwInfo = drm->getRootDeviceEnvironment().getHardwareInfo();
    return LocalMemoryHelper::get(pHwInfo->platform.eProductFamily)->createGemExt(drm, data, dataSize, allocSize, handle);
}

void MemoryInfoImpl::assignRegionsFromDistances(const void *distanceInfosPtr, size_t size) {
}

} // namespace NEO
