/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/ioctl_helper.h"

#include "shared/source/os_interface/linux/drm_neo.h"

namespace NEO {

uint32_t IoctlHelper::ioctl(Drm *drm, unsigned long request, void *arg) {
    return drm->ioctl(request, arg);
}

std::string IoctlHelper::getIoctlParamString(int param) {
    switch (param) {
    case I915_PARAM_CHIPSET_ID:
        return "I915_PARAM_CHIPSET_ID";
    case I915_PARAM_REVISION:
        return "I915_PARAM_REVISION";
    case I915_PARAM_HAS_EXEC_SOFTPIN:
        return "I915_PARAM_HAS_EXEC_SOFTPIN";
    case I915_PARAM_HAS_POOLED_EU:
        return "I915_PARAM_HAS_POOLED_EU";
    case I915_PARAM_HAS_SCHEDULER:
        return "I915_PARAM_HAS_SCHEDULER";
    case I915_PARAM_EU_TOTAL:
        return "I915_PARAM_EU_TOTAL";
    case I915_PARAM_SUBSLICE_TOTAL:
        return "I915_PARAM_SUBSLICE_TOTAL";
    case I915_PARAM_MIN_EU_IN_POOL:
        return "I915_PARAM_MIN_EU_IN_POOL";
    case I915_PARAM_CS_TIMESTAMP_FREQUENCY:
        return "I915_PARAM_CS_TIMESTAMP_FREQUENCY";
    default:
        return std::to_string(param);
    }
}

std::string IoctlHelper::getIoctlString(unsigned long request) {
    switch (request) {
    case DRM_IOCTL_I915_GEM_EXECBUFFER2:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2";
    case DRM_IOCTL_I915_GEM_WAIT:
        return "DRM_IOCTL_I915_GEM_WAIT";
    case DRM_IOCTL_GEM_CLOSE:
        return "DRM_IOCTL_GEM_CLOSE";
    case DRM_IOCTL_I915_GEM_USERPTR:
        return "DRM_IOCTL_I915_GEM_USERPTR";
    case DRM_IOCTL_I915_INIT:
        return "DRM_IOCTL_I915_INIT";
    case DRM_IOCTL_I915_FLUSH:
        return "DRM_IOCTL_I915_FLUSH";
    case DRM_IOCTL_I915_FLIP:
        return "DRM_IOCTL_I915_FLIP";
    case DRM_IOCTL_I915_BATCHBUFFER:
        return "DRM_IOCTL_I915_BATCHBUFFER";
    case DRM_IOCTL_I915_IRQ_EMIT:
        return "DRM_IOCTL_I915_IRQ_EMIT";
    case DRM_IOCTL_I915_IRQ_WAIT:
        return "DRM_IOCTL_I915_IRQ_WAIT";
    case DRM_IOCTL_I915_GETPARAM:
        return "DRM_IOCTL_I915_GETPARAM";
    case DRM_IOCTL_I915_SETPARAM:
        return "DRM_IOCTL_I915_SETPARAM";
    case DRM_IOCTL_I915_ALLOC:
        return "DRM_IOCTL_I915_ALLOC";
    case DRM_IOCTL_I915_FREE:
        return "DRM_IOCTL_I915_FREE";
    case DRM_IOCTL_I915_INIT_HEAP:
        return "DRM_IOCTL_I915_INIT_HEAP";
    case DRM_IOCTL_I915_CMDBUFFER:
        return "DRM_IOCTL_I915_CMDBUFFER";
    case DRM_IOCTL_I915_DESTROY_HEAP:
        return "DRM_IOCTL_I915_DESTROY_HEAP";
    case DRM_IOCTL_I915_SET_VBLANK_PIPE:
        return "DRM_IOCTL_I915_SET_VBLANK_PIPE";
    case DRM_IOCTL_I915_GET_VBLANK_PIPE:
        return "DRM_IOCTL_I915_GET_VBLANK_PIPE";
    case DRM_IOCTL_I915_VBLANK_SWAP:
        return "DRM_IOCTL_I915_VBLANK_SWAP";
    case DRM_IOCTL_I915_HWS_ADDR:
        return "DRM_IOCTL_I915_HWS_ADDR";
    case DRM_IOCTL_I915_GEM_INIT:
        return "DRM_IOCTL_I915_GEM_INIT";
    case DRM_IOCTL_I915_GEM_EXECBUFFER:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER";
    case DRM_IOCTL_I915_GEM_EXECBUFFER2_WR:
        return "DRM_IOCTL_I915_GEM_EXECBUFFER2_WR";
    case DRM_IOCTL_I915_GEM_PIN:
        return "DRM_IOCTL_I915_GEM_PIN";
    case DRM_IOCTL_I915_GEM_UNPIN:
        return "DRM_IOCTL_I915_GEM_UNPIN";
    case DRM_IOCTL_I915_GEM_BUSY:
        return "DRM_IOCTL_I915_GEM_BUSY";
    case DRM_IOCTL_I915_GEM_SET_CACHING:
        return "DRM_IOCTL_I915_GEM_SET_CACHING";
    case DRM_IOCTL_I915_GEM_GET_CACHING:
        return "DRM_IOCTL_I915_GEM_GET_CACHING";
    case DRM_IOCTL_I915_GEM_THROTTLE:
        return "DRM_IOCTL_I915_GEM_THROTTLE";
    case DRM_IOCTL_I915_GEM_ENTERVT:
        return "DRM_IOCTL_I915_GEM_ENTERVT";
    case DRM_IOCTL_I915_GEM_LEAVEVT:
        return "DRM_IOCTL_I915_GEM_LEAVEVT";
    case DRM_IOCTL_I915_GEM_CREATE:
        return "DRM_IOCTL_I915_GEM_CREATE";
    case DRM_IOCTL_I915_GEM_PREAD:
        return "DRM_IOCTL_I915_GEM_PREAD";
    case DRM_IOCTL_I915_GEM_PWRITE:
        return "DRM_IOCTL_I915_GEM_PWRITE";
    case DRM_IOCTL_I915_GEM_SET_DOMAIN:
        return "DRM_IOCTL_I915_GEM_SET_DOMAIN";
    case DRM_IOCTL_I915_GEM_SW_FINISH:
        return "DRM_IOCTL_I915_GEM_SW_FINISH";
    case DRM_IOCTL_I915_GEM_SET_TILING:
        return "DRM_IOCTL_I915_GEM_SET_TILING";
    case DRM_IOCTL_I915_GEM_GET_TILING:
        return "DRM_IOCTL_I915_GEM_GET_TILING";
    case DRM_IOCTL_I915_GEM_GET_APERTURE:
        return "DRM_IOCTL_I915_GEM_GET_APERTURE";
    case DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID:
        return "DRM_IOCTL_I915_GET_PIPE_FROM_CRTC_ID";
    case DRM_IOCTL_I915_GEM_MADVISE:
        return "DRM_IOCTL_I915_GEM_MADVISE";
    case DRM_IOCTL_I915_OVERLAY_PUT_IMAGE:
        return "DRM_IOCTL_I915_OVERLAY_PUT_IMAGE";
    case DRM_IOCTL_I915_OVERLAY_ATTRS:
        return "DRM_IOCTL_I915_OVERLAY_ATTRS";
    case DRM_IOCTL_I915_SET_SPRITE_COLORKEY:
        return "DRM_IOCTL_I915_SET_SPRITE_COLORKEY";
    case DRM_IOCTL_I915_GET_SPRITE_COLORKEY:
        return "DRM_IOCTL_I915_GET_SPRITE_COLORKEY";
    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE:
        return "DRM_IOCTL_I915_GEM_CONTEXT_CREATE";
    case DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT:
        return "DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT";
    case DRM_IOCTL_I915_GEM_CONTEXT_DESTROY:
        return "DRM_IOCTL_I915_GEM_CONTEXT_DESTROY";
    case DRM_IOCTL_I915_REG_READ:
        return "DRM_IOCTL_I915_REG_READ";
    case DRM_IOCTL_I915_GET_RESET_STATS:
        return "DRM_IOCTL_I915_GET_RESET_STATS";
    case DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM:
        return "DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM";
    case DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM:
        return "DRM_IOCTL_I915_GEM_CONTEXT_SETPARAM";
    case DRM_IOCTL_I915_PERF_OPEN:
        return "DRM_IOCTL_I915_PERF_OPEN";
    case DRM_IOCTL_I915_PERF_ADD_CONFIG:
        return "DRM_IOCTL_I915_PERF_ADD_CONFIG";
    case DRM_IOCTL_I915_PERF_REMOVE_CONFIG:
        return "DRM_IOCTL_I915_PERF_REMOVE_CONFIG";
    case DRM_IOCTL_I915_QUERY:
        return "DRM_IOCTL_I915_QUERY";
    case DRM_IOCTL_I915_GEM_MMAP:
        return "DRM_IOCTL_I915_GEM_MMAP";
    case DRM_IOCTL_PRIME_FD_TO_HANDLE:
        return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
    case DRM_IOCTL_PRIME_HANDLE_TO_FD:
        return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
    default:
        return std::to_string(request);
    }
}
} // namespace NEO
