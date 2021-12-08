/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

namespace NEO {

uint32_t IoctlHelperPrelim20::createGemExt(Drm *drm, void *data, uint32_t dataSize, size_t allocSize, uint32_t &handle) {
    prelim_drm_i915_gem_object_param regionParam{};
    regionParam.size = dataSize;
    regionParam.data = reinterpret_cast<uintptr_t>(data);
    regionParam.param = PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS;

    prelim_drm_i915_gem_create_ext_setparam setparamRegion{};
    setparamRegion.base.name = PRELIM_I915_GEM_CREATE_EXT_SETPARAM;
    setparamRegion.param = regionParam;

    prelim_drm_i915_gem_create_ext createExt{};
    createExt.size = allocSize;
    createExt.extensions = reinterpret_cast<uintptr_t>(&setparamRegion);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "Performing GEM_CREATE_EXT with { size: %lu, param: 0x%llX",
                     allocSize, regionParam.param);

    if (DebugManager.flags.PrintBOCreateDestroyResult.get()) {
        for (uint32_t i = 0; i < dataSize; i++) {
            auto region = reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(data)[i];
            printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, ", memory class: %d, memory instance: %d",
                             region.memory_class, region.memory_instance);
        }
        printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "%s", " }\n");
    }

    auto ret = IoctlHelper::ioctl(drm, PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT, &createExt);

    printDebugString(DebugManager.flags.PrintBOCreateDestroyResult.get(), stdout, "GEM_CREATE_EXT has returned: %d BO-%u with size: %lu\n", ret, createExt.handle, createExt.size);
    handle = createExt.handle;
    return ret;
}

std::unique_ptr<uint8_t[]> IoctlHelperPrelim20::translateIfRequired(uint8_t *dataQuery, int32_t length) {
    return std::unique_ptr<uint8_t[]>(dataQuery);
}

} // namespace NEO
