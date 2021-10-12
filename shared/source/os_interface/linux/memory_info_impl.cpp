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

uint32_t MemoryInfoImpl::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu", allocSize);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < dataSize; i++) {
            auto region = reinterpret_cast<drm_i915_gem_memory_class_instance *>(data)[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    if (createGemExtMemoryRegions(drm, data, dataSize, allocSize, handle) == 0) {
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: %d BO-%u with size: %lu\n", 0, handle, allocSize);
        return 0;
    }

    drm_i915_gem_object_param regionParam{};
    regionParam.size = dataSize;
    regionParam.data = reinterpret_cast<uintptr_t>(data);
    regionParam.param = I915_OBJECT_PARAM | I915_PARAM_MEMORY_REGIONS;

    drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);

    auto ret = drm->ioctl(DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_SETPARAM has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

void MemoryInfoImpl::assignRegionsFromDistances(const void *distanceInfosPtr, size_t size) {
}

} // namespace NEO
