/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/os_interface/linux/device_command_stream_fixture_context.h"

#include "shared/source/os_interface/linux/i915_prelim.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"

int DrmMockCustomPrelimContext::ioctlExtra(DrmIoctl request, void *arg) {
    switch (request) {
    case DrmIoctl::GemWaitUserFence: {
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
        UNRECOVERABLE_IF(true);
    } break;
    }
    return 0;
}

void DrmMockCustomPrelimContext::execBufferExtensions(void *arg) {
    const auto execbuf = reinterpret_cast<NEO::MockExecBuffer *>(arg);
    if ((execbuf->hasUseExtensionsFlag()) &&
        (execbuf->getCliprectsPtr() != 0)) {
        DrmUserExtension *base = reinterpret_cast<DrmUserExtension *>(execbuf->getCliprectsPtr());
        if (base->name == PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE) {
            prelim_drm_i915_gem_execbuffer_ext_user_fence *userFenceExt =
                reinterpret_cast<prelim_drm_i915_gem_execbuffer_ext_user_fence *>(execbuf->getCliprectsPtr());
            this->completionAddress = userFenceExt->addr;
            this->completionValue = userFenceExt->value;
        }
    }
}
