/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "drm/i915_drm.h"
#include "engine_node.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <atomic>
#include <cstdint>
#include <iostream>

using NEO::Drm;
using NEO::HwDeviceIdDrm;
using NEO::RootDeviceEnvironment;

extern const int mockFd;
extern const char *mockPciPath;

class Ioctls {
  public:
    Ioctls() {
        reset();
    }
    void reset();
    std::atomic<int32_t> total;
    std::atomic<int32_t> query;
    std::atomic<int32_t> execbuffer2;
    std::atomic<int32_t> gemUserptr;
    std::atomic<int32_t> gemCreate;
    std::atomic<int32_t> gemSetTiling;
    std::atomic<int32_t> gemGetTiling;
    std::atomic<int32_t> gemGetAperture;
    std::atomic<int32_t> primeFdToHandle;
    std::atomic<int32_t> handleToPrimeFd;
    std::atomic<int32_t> gemMmap;
    std::atomic<int32_t> gemMmapOffset;
    std::atomic<int32_t> gemSetDomain;
    std::atomic<int32_t> gemWait;
    std::atomic<int32_t> gemClose;
    std::atomic<int32_t> gemResetStats;
    std::atomic<int32_t> regRead;
    std::atomic<int32_t> getParam;
    std::atomic<int32_t> contextGetParam;
    std::atomic<int32_t> contextSetParam;
    std::atomic<int32_t> contextCreate;
    std::atomic<int32_t> contextDestroy;
};

class DrmMockSuccess : public Drm {
  public:
    using Drm::setupIoctlHelper;
    DrmMockSuccess(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, mockPciPath), rootDeviceEnvironment) {}

    int ioctl(unsigned long request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {}

    int ioctl(unsigned long request, void *arg) override { return -1; };
};

class DrmMockTime : public DrmMockSuccess {
  public:
    using DrmMockSuccess::DrmMockSuccess;
    int ioctl(unsigned long request, void *arg) override {
        drm_i915_reg_read *reg = reinterpret_cast<drm_i915_reg_read *>(arg);
        reg->val = getVal() << 32;
        return 0;
    };

    uint64_t getVal() {
        static uint64_t val = 0;
        return ++val;
    }
};

class DrmMockCustom : public Drm {
  public:
    using Drm::bindAvailable;
    using Drm::cacheInfo;
    using Drm::completionFenceSupported;
    using Drm::memoryInfo;
    using Drm::setupIoctlHelper;

    struct IoctlResExt {
        std::vector<int32_t> no;
        int32_t res;

        IoctlResExt(int32_t no, int32_t res) : no(1u, no), res(res) {}
    };

    struct WaitUserFenceCall {
        uint64_t address = 0u;
        uint64_t value = 0u;
        uint32_t ctxId = 0u;
        ValueWidth dataWidth = ValueWidth::U8;
        int64_t timeout = 0;
        uint16_t flags = 0;

        uint32_t called = 0u;
    };

    struct IsVmBindAvailableCall {
        bool callParent = true;
        bool returnValue = true;
        uint32_t called = 0u;
    };

    DrmMockCustom(RootDeviceEnvironment &rootDeviceEnvironment);

    int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) override;

    bool isVmBindAvailable() override;

    bool completionFenceSupport() override {
        return completionFenceSupported;
    }

    void testIoctls();

    int ioctl(unsigned long request, void *arg) override;

    virtual int ioctlExtra(unsigned long request, void *arg) {
        return -1;
    }

    int getErrno() override {
        return errnoValue;
    }

    void reset() {
        ioctl_res = 0;
        ioctl_cnt.reset();
        ioctl_expected.reset();
        ioctl_res_ext = &NONE;
    }

    virtual void execBufferExtensions(void *execbuf) {
    }

    Ioctls ioctl_cnt{};
    Ioctls ioctl_expected{};

    IoctlResExt NONE = {-1, 0};

    WaitUserFenceCall waitUserFenceCall{};
    IsVmBindAvailableCall isVmBindAvailableCall{};

    std::atomic<int> ioctl_res;
    std::atomic<IoctlResExt *> ioctl_res_ext;

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    drm_i915_gem_execbuffer2 execBuffer = {0};

    //First exec object
    drm_i915_gem_exec_object2 execBufferBufferObjects = {0};

    //DRM_IOCTL_I915_GEM_CREATE
    __u64 createParamsSize = 0;
    __u32 createParamsHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_TILING
    __u32 setTilingMode = 0;
    __u32 setTilingHandle = 0;
    __u32 setTilingStride = 0;
    //DRM_IOCTL_I915_GEM_GET_TILING
    __u32 getTilingModeOut = I915_TILING_NONE;
    __u32 getTilingHandleIn = 0;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE
    __u32 outputHandle = 0;
    __s32 inputFd = 0;
    //DRM_IOCTL_PRIME_HANDLE_TO_FD
    __u32 inputHandle = 0;
    __s32 outputFd = 0;
    __s32 inputFlags = 0;
    //DRM_IOCTL_I915_GEM_USERPTR
    __u32 returnHandle = 0;
    //DRM_IOCTL_I915_GEM_MMAP
    __u32 mmapHandle = 0;
    __u32 mmapPad = 0;
    __u64 mmapOffset = 0;
    __u64 mmapSize = 0;
    __u64 mmapAddrPtr = 0x7F4000001000;
    __u64 mmapFlags = 0;
    //DRM_IOCTL_I915_GEM_SET_DOMAIN
    __u32 setDomainHandle = 0;
    __u32 setDomainReadDomains = 0;
    __u32 setDomainWriteDomain = 0;
    //DRM_IOCTL_I915_GETPARAM
    drm_i915_getparam_t recordedGetParam = {0};
    int getParamRetValue = 0;
    //DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM
    drm_i915_gem_context_param recordedGetContextParam = {0};
    __u64 getContextParamRetValue = 0;
    //DRM_IOCTL_I915_GEM_WAIT
    int64_t gemWaitTimeout = 0;
    //DRM_IOCTL_I915_GEM_MMAP_OFFSET
    __u32 mmapOffsetHandle = 0;
    __u32 mmapOffsetPad = 0;
    __u64 mmapOffsetExpected = 0;
    __u64 mmapOffsetFlags = 0;
    bool failOnMmapOffset = false;

    int errnoValue = 0;

    bool returnIoctlExtraErrorValue = false;
};
