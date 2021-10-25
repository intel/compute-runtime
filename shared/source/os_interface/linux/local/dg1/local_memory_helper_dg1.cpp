/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/local_memory_helper.h"

#include "third_party/uapi/dg1/drm/i915_drm.h"

namespace NEO {
constexpr static auto gfxProduct = IGFX_DG1;

extern uint32_t createGemExtMemoryRegions(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle);
extern bool isQueryDrmTip(uint8_t *dataQuery, int32_t length);
extern std::unique_ptr<uint8_t[]> translateToDrmTip(uint8_t *dataQuery, int32_t &length);

template <>
uint32_t LocalMemoryHelperImpl<gfxProduct>::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
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
    handle = 0u;

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

    auto ret = LocalMemoryHelper::ioctl(drm, DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    handle = createExt.handle;
    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT with EXT_SETPARAM has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    return ret;
}

template <>
std::unique_ptr<uint8_t[]> LocalMemoryHelperImpl<gfxProduct>::translateIfRequired(uint8_t *dataQuery, int32_t length) {
    if (isQueryDrmTip(dataQuery, length)) {
        DEBUG_BREAK_IF(true);
        return std::unique_ptr<uint8_t[]>(dataQuery);
    }
    auto data = std::unique_ptr<uint8_t[]>(dataQuery);
    return translateToDrmTip(data.get(), length);
}

template class LocalMemoryHelperImpl<gfxProduct>;
} // namespace NEO
