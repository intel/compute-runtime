/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/uapi/prelim/drm/i915_drm.h"

#include <string>
#include <sys/ioctl.h>

namespace NEO {

namespace IoctlToStringHelper {
std::string getIoctlStringRemaining(unsigned long request) {
    switch (request) {
    case PRELIM_DRM_IOCTL_I915_GEM_VM_BIND:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_BIND";
    case PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND";
    case PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE:
        return "PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE";
    case PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT:
        return "PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT";
    case PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_ADVISE";
    case PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH:
        return "PRELIM_DRM_IOCTL_I915_GEM_VM_PREFETCH";
    case PRELIM_DRM_IOCTL_I915_UUID_REGISTER:
        return "PRELIM_DRM_IOCTL_I915_UUID_REGISTER";
    case PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER:
        return "PRELIM_DRM_IOCTL_I915_UUID_UNREGISTER";
    case PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN:
        return "PRELIM_DRM_IOCTL_I915_DEBUGGER_OPEN";
    case PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_RESERVE";
    case PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE:
        return "PRELIM_DRM_IOCTL_I915_GEM_CLOS_FREE";
    case PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE:
        return "PRELIM_DRM_IOCTL_I915_GEM_CACHE_RESERVE";
    case DRM_IOCTL_I915_GEM_MMAP_GTT:
        return "DRM_IOCTL_I915_GEM_MMAP_GTT";
    case DRM_IOCTL_I915_GEM_MMAP_OFFSET:
        return "DRM_IOCTL_I915_GEM_MMAP_OFFSET";
    case DRM_IOCTL_I915_GEM_VM_CREATE:
        return "DRM_IOCTL_I915_GEM_VM_CREATE";
    case DRM_IOCTL_I915_GEM_VM_DESTROY:
        return "DRM_IOCTL_I915_GEM_VM_DESTROY";
    default:
        return std::to_string(request);
    }
}

std::string getIoctlParamStringRemaining(int param) {
    switch (param) {
    case PRELIM_I915_PARAM_HAS_VM_BIND:
        return "PRELIM_I915_PARAM_HAS_VM_BIND";
    default:
        return std::to_string(param);
    }
}
} // namespace IoctlToStringHelper

} // namespace NEO
