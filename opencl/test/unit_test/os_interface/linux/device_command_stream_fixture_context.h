/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/libult/linux/drm_mock_prelim_context.h"

struct DrmMockCustomPrelimContext {
    //PRELIM_DRM_IOCTL_I915_GEM_CREATE_EXT
    uint64_t createExtSize = 0;
    uint32_t createExtHandle = 0;
    uint64_t createExtExtensions = 0;

    //PRELIM_DRM_IOCTL_I915_GEM_WAIT_USER_FENCE
    WaitUserFence receivedGemWaitUserFence{};
    uint32_t gemWaitUserFenceCalled = 0;

    //PRELIM_DRM_I915_GEM_EXECBUFFER_EXT_USER_FENCE
    uint64_t completionAddress = 0;
    uint64_t completionValue = 0;

    int ioctlExtra(unsigned long request, void *arg);
    void execBufferExtensions(void *arg);
};
