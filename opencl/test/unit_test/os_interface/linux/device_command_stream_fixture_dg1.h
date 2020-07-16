/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"

class DrmMockCustomDg1 : public DrmMockCustom {
  public:
    using Drm::memoryInfo;

    class IoctlsDg1 {
      public:
        void reset() {
            gemCreateExt = 0;
            gemMmapOffset = 0;
        }
        std::atomic<int32_t> gemCreateExt;
        std::atomic<int32_t> gemMmapOffset;
    };

    IoctlsDg1 ioctlDg1_cnt;
    IoctlsDg1 ioctlDg1_expected;

    void testIoctlsDg1() {
#define NEO_IOCTL_EXPECT_EQ(PARAM)                                          \
    if (this->ioctlDg1_expected.PARAM >= 0) {                               \
        EXPECT_EQ(this->ioctlDg1_expected.PARAM, this->ioctlDg1_cnt.PARAM); \
    }
        NEO_IOCTL_EXPECT_EQ(gemMmapOffset);
#undef NEO_IOCTL_EXPECT_EQ
    }

    //DRM_IOCTL_I915_GEM_CREATE_EXT
    __u64 createExtSize = 0;
    __u32 createExtHandle = 0;
    __u64 createExtExtensions = 0;

    //DRM_IOCTL_I915_GEM_MMAP_OFFSET
    __u32 mmapOffsetHandle = 0;
    __u32 mmapOffsetPad = 0;
    __u64 mmapOffsetOffset = 0;
    __u64 mmapOffsetFlags = 0;

    int ioctlExtra(unsigned long request, void *arg) override {
        switch (request) {
        case DRM_IOCTL_I915_GEM_CREATE_EXT: {
            auto createExtParams = reinterpret_cast<drm_i915_gem_create_ext *>(arg);
            createExtSize = createExtParams->size;
            createExtHandle = createExtParams->handle;
            createExtExtensions = createExtParams->extensions;
            ioctlDg1_cnt.gemCreateExt++;
        } break;
        case DRM_IOCTL_I915_GEM_MMAP_OFFSET: {
            auto mmapOffsetParams = reinterpret_cast<drm_i915_gem_mmap_offset *>(arg);
            mmapOffsetHandle = mmapOffsetParams->handle;
            mmapOffsetPad = mmapOffsetParams->pad;
            mmapOffsetOffset = mmapOffsetParams->offset;
            mmapOffsetFlags = mmapOffsetParams->flags;
            ioctlDg1_cnt.gemMmapOffset++;
        } break;
        default: {
            std::cout << std::hex << DRM_IOCTL_I915_GEM_WAIT << std::endl;
            std::cout << "unexpected IOCTL: " << std::hex << request << std::endl;
            UNRECOVERABLE_IF(true);
        } break;
        }
        return 0;
    }

    DrmMockCustomDg1() : DrmMockCustom() {
        ioctlDg1_cnt.reset();
        ioctlDg1_expected.reset();
    }
};
