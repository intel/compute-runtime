/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "third_party/uapi/xe_hp_sdv/drm/i915_drm.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_XE_HP_SDV;

template <>
uint32_t LocalMemoryHelperImpl<gfxProduct>::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    drm_i915_gem_create_ext_memory_regions memRegions;
    memRegions.num_regions = dataSize;
    memRegions.regions = reinterpret_cast<uintptr_t>(data);
    memRegions.base.name = I915_GEM_CREATE_EXT_MEMORY_REGIONS;

    drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&memRegions);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu",
                     allocSize);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < dataSize; i++) {
            auto region = reinterpret_cast<drm_i915_gem_memory_class_instance *>(data)[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_MEMORY_REGIONS has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

template <>
std::unique_ptr<uint8_t[]> LocalMemoryHelperImpl<gfxProduct>::translateIfRequired(uint8_t *dataQuery, int32_t length) {
    return std::unique_ptr<uint8_t[]>(dataQuery);
}

template class LocalMemoryHelperImpl<gfxProduct>;

} // namespace NEO
