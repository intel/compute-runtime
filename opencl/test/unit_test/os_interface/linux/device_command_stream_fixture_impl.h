/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

class DrmMockCustomImpl : public DrmMockCustom {
  public:
    using Drm::memoryInfo;

    class Ioctls {
      public:
        void reset() {
            gemCreateExt = 0;
            gemMmapOffset = 0;
        }
        std::atomic<int32_t> gemCreateExt;
        std::atomic<int32_t> gemMmapOffset;
    };

    Ioctls ioctlImpl_cnt;
    Ioctls ioctlImpl_expected;

    void testIoctls() {
#define NEO_IOCTL_EXPECT_EQ(PARAM)                                            \
    if (this->ioctlImpl_expected.PARAM >= 0) {                                \
        EXPECT_EQ(this->ioctlImpl_expected.PARAM, this->ioctlImpl_cnt.PARAM); \
    }
        NEO_IOCTL_EXPECT_EQ(gemMmapOffset);
#undef NEO_IOCTL_EXPECT_EQ
    }

    //DRM_IOCTL_I915_GEM_CREATE_EXT
    __u64 createExtSize = 0;
    __u32 createExtHandle = 0;
    __u64 createExtExtensions = 0;

    int ioctlExtra(unsigned long request, void *arg) override {
        switch (request) {
        case DRM_IOCTL_I915_GEM_CREATE_EXT: {
            auto createExtParams = reinterpret_cast<drm_i915_gem_create_ext *>(arg);
            createExtSize = createExtParams->size;
            createExtHandle = createExtParams->handle;
            createExtExtensions = createExtParams->extensions;
            ioctlImpl_cnt.gemCreateExt++;
        } break;
        default: {
            std::cout << "unexpected IOCTL: " << std::hex << request << std::endl;
            UNRECOVERABLE_IF(true);
        } break;
        }
        return 0;
    }

    DrmMockCustomImpl(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockCustom(rootDeviceEnvironment) {
        ioctlImpl_cnt.reset();
        ioctlImpl_expected.reset();
    }
};
