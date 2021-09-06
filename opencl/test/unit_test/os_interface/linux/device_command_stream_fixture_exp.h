/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/os_interface/linux/device_command_stream_fixture.h"

class DrmMockCustomExp : public DrmMockCustom {
  public:
    using Drm::memoryInfo;

    class IoctlsExp {
      public:
        void reset() {
            gemCreateExt = 0;
            gemMmapOffset = 0;
        }
        std::atomic<int32_t> gemCreateExt;
        std::atomic<int32_t> gemMmapOffset;
    };

    IoctlsExp ioctlExp_cnt;
    IoctlsExp ioctlExp_expected;

    void testIoctlsExp() {
#define NEO_IOCTL_EXPECT_EQ(PARAM)                                          \
    if (this->ioctlExp_expected.PARAM >= 0) {                               \
        EXPECT_EQ(this->ioctlExp_expected.PARAM, this->ioctlExp_cnt.PARAM); \
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

    bool failOnMmapOffset = false;

    int ioctlExtra(unsigned long request, void *arg) override {
        switch (request) {
        case DRM_IOCTL_I915_GEM_CREATE_EXT: {
            auto createExtParams = reinterpret_cast<drm_i915_gem_create_ext *>(arg);
            createExtSize = createExtParams->size;
            createExtHandle = createExtParams->handle;
            createExtExtensions = createExtParams->extensions;
            ioctlExp_cnt.gemCreateExt++;
        } break;
        case DRM_IOCTL_I915_GEM_MMAP_OFFSET: {
            auto mmapOffsetParams = reinterpret_cast<drm_i915_gem_mmap_offset *>(arg);
            mmapOffsetHandle = mmapOffsetParams->handle;
            mmapOffsetPad = mmapOffsetParams->pad;
            mmapOffsetOffset = mmapOffsetParams->offset;
            mmapOffsetFlags = mmapOffsetParams->flags;
            ioctlExp_cnt.gemMmapOffset++;
            if (failOnMmapOffset == true) {
                return -1;
            }
        } break;
        default: {
            std::cout << "unexpected IOCTL: " << std::hex << request << std::endl;
            UNRECOVERABLE_IF(true);
        } break;
        }
        return 0;
    }

    DrmMockCustomExp() : DrmMockCustom() {
        ioctlExp_cnt.reset();
        ioctlExp_expected.reset();
    }
};
