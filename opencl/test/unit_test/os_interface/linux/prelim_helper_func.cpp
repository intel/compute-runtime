/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

#include <sys/ioctl.h>

int handlePrelimRequests(unsigned long request, void *arg, int ioctlRetVal) {
    if (request == PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT) {
        auto createExtParams = static_cast<prelim_drm_i915_gem_create_ext *>(arg);
        if (createExtParams->size == 0) {
            return EINVAL;
        }
        createExtParams->handle = 1u;
        auto extensions = reinterpret_cast<prelim_drm_i915_gem_create_ext_setparam *>(createExtParams->extensions);
        if (extensions == nullptr) {
            return EINVAL;
        }
        auto setparamRegion = *extensions;
        if (setparamRegion.base.name != PRELIM_I915_GEM_CREATE_EXT_SETPARAM) {
            return EINVAL;
        }
        if ((setparamRegion.param.size == 0) ||
            (setparamRegion.param.param != (PRELIM_I915_OBJECT_PARAM | PRELIM_I915_PARAM_MEMORY_REGIONS))) {
            return EINVAL;
        }
        auto data = reinterpret_cast<prelim_drm_i915_gem_memory_class_instance *>(setparamRegion.param.data);
        if (data == nullptr) {
            return EINVAL;
        }

        if ((data->memory_class != PRELIM_I915_MEMORY_CLASS_SYSTEM) && (data->memory_class != PRELIM_I915_MEMORY_CLASS_DEVICE)) {
            return EINVAL;
        }
    } else if (request == PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE) {
        auto closReserveArg = static_cast<prelim_drm_i915_gem_clos_reserve *>(arg);
        closReserveArg->clos_index = 1u;
    }
    return ioctlRetVal;
}
