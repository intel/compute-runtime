/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture_context.h"

#include "third_party/uapi/prelim/drm/i915_drm.h"

int DrmMockCustomPrelimContext::ioctlExtra(unsigned long request, void *arg) {
    switch (request) {
    case PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT: {
        auto createExtParams = reinterpret_cast<prelim_drm_i915_gem_create_ext *>(arg);
        createExtSize = createExtParams->size;
        createExtHandle = createExtParams->handle;
        createExtExtensions = createExtParams->extensions;
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_VM_BIND: {
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_VM_UNBIND: {
    } break;
    case PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE: {
        const auto wait = reinterpret_cast<prelim_drm_i915_gem_wait_user_fence *>(arg);
        receivedGemWaitUserFence = WaitUserFence{
            wait->extensions,
            wait->addr,
            wait->ctx_id,
            wait->op,
            wait->flags,
            wait->value,
            wait->mask,
            wait->timeout,
        };
        gemWaitUserFenceCalled++;
    } break;
    default: {
        std::cout << std::hex << DRM_IOCTL_I915_GEM_WAIT << std::endl;
        std::cout << "unexpected IOCTL: " << std::hex << request << std::endl;
        UNRECOVERABLE_IF(true);
    } break;
    }
    return 0;
}

void DrmMockCustomPrelimContext::execBufferExtensions(void *arg) {
    const auto execbuf = reinterpret_cast<drm_i915_gem_execbuffer2 *>(arg);
    if ((execbuf->flags | I915_EXEC_USE_EXTENSIONS) &&
        (execbuf->cliprects_ptr != 0)) {
        i915_user_extension *base = reinterpret_cast<i915_user_extension *>(execbuf->cliprects_ptr);
        if (base->name == PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE) {
            prelim_drm_i915_gem_execbuffer_ext_user_fence *userFenceExt =
                reinterpret_cast<prelim_drm_i915_gem_execbuffer_ext_user_fence *>(execbuf->cliprects_ptr);
            this->completionAddress = userFenceExt->addr;
            this->completionValue = userFenceExt->value;
        }
    }
}
