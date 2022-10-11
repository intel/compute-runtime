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
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"

#include "engine_node.h"

#include <atomic>
#include <cstdint>

using NEO::Drm;
using NEO::DrmIoctl;
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
    std::atomic<int32_t> gemCreateExt;
    std::atomic<int32_t> gemSetTiling;
    std::atomic<int32_t> gemGetTiling;
    std::atomic<int32_t> gemVmCreate;
    std::atomic<int32_t> gemVmDestroy;
    std::atomic<int32_t> primeFdToHandle;
    std::atomic<int32_t> handleToPrimeFd;
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
    DrmMockSuccess(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, mockPciPath), rootDeviceEnvironment) {
        setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
    }

    int ioctl(DrmIoctl request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
        setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
    }

    int ioctl(DrmIoctl request, void *arg) override { return -1; };
};

class DrmMockTime : public DrmMockSuccess {
  public:
    using DrmMockSuccess::DrmMockSuccess;
    int ioctl(DrmIoctl request, void *arg) override {
        auto *reg = reinterpret_cast<NEO::RegisterRead *>(arg);
        reg->value = getVal() << 32;
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

    bool getSetPairAvailable() override;

    bool isVmBindAvailable() override;

    bool completionFenceSupport() override {
        return completionFenceSupported;
    }

    void testIoctls();

    int ioctl(DrmIoctl request, void *arg) override;

    virtual int ioctlExtra(DrmIoctl request, void *arg) {
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
    IsVmBindAvailableCall getSetPairAvailableCall{};
    IsVmBindAvailableCall isVmBindAvailableCall{};

    std::atomic<int> ioctl_res;
    std::atomic<IoctlResExt *> ioctl_res_ext;

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    NEO::MockExecBuffer execBuffer{};

    //First exec object
    NEO::MockExecObject execBufferBufferObjects{};

    //DRM_IOCTL_I915_GEM_CREATE
    uint64_t createParamsSize = 0;
    uint32_t createParamsHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_TILING
    uint32_t setTilingMode = 0;
    uint32_t setTilingHandle = 0;
    uint32_t setTilingStride = 0;
    //DRM_IOCTL_I915_GEM_GET_TILING
    uint32_t getTilingModeOut = 0;
    uint32_t getTilingHandleIn = 0;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE
    uint32_t outputHandle = 0;
    int32_t inputFd = 0;
    //DRM_IOCTL_PRIME_HANDLE_TO_FD
    uint32_t inputHandle = 0;
    int32_t outputFd = 0;
    bool incrementOutputFdAfterCall = false;
    int32_t inputFlags = 0;
    //DRM_IOCTL_I915_GEM_USERPTR
    uint32_t returnHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_DOMAIN
    uint32_t setDomainHandle = 0;
    uint32_t setDomainReadDomains = 0;
    uint32_t setDomainWriteDomain = 0;
    //DRM_IOCTL_I915_GETPARAM
    NEO::GetParam recordedGetParam = {0};
    int getParamRetValue = 0;
    //DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM
    NEO::GemContextParam recordedGetContextParam = {0};
    uint64_t getContextParamRetValue = 0;
    //DRM_IOCTL_I915_GEM_WAIT
    int64_t gemWaitTimeout = 0;
    //DRM_IOCTL_I915_GEM_MMAP_OFFSET
    uint32_t mmapOffsetHandle = 0;
    uint32_t mmapOffsetPad = 0;
    uint64_t mmapOffsetExpected = 0;
    uint64_t mmapOffsetFlags = 0;
    bool failOnMmapOffset = false;
    bool failOnPrimeFdToHandle = false;

    //DRM_IOCTL_I915_GEM_CREATE_EXT
    uint64_t createExtSize = 0;
    uint32_t createExtHandle = 0;
    uint64_t createExtExtensions = 0;

    int errnoValue = 0;

    bool returnIoctlExtraErrorValue = false;
};
