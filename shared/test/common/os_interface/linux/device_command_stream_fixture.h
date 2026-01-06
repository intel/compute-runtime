/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"

#include <atomic>
#include <cstdint>

using NEO::Drm;
using NEO::DrmIoctl;
using NEO::HwDeviceIdDrm;
using NEO::OsContext;
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
    std::atomic<int32_t> syncObjFdToHandle;
    std::atomic<int32_t> syncObjWait;
    std::atomic<int32_t> syncObjSignal;
    std::atomic<int32_t> syncObjTimelineWait;
    std::atomic<int32_t> syncObjTimelineSignal;
    std::atomic<int32_t> gemMmapOffset;
    std::atomic<int32_t> gemSetDomain;
    std::atomic<int32_t> gemWait;
    std::atomic<int32_t> gemClose;
    std::atomic<int32_t> getResetStats;
    std::atomic<int32_t> regRead;
    std::atomic<int32_t> getParam;
    std::atomic<int32_t> contextGetParam;
    std::atomic<int32_t> contextSetParam;
    std::atomic<int32_t> contextCreate;
    std::atomic<int32_t> contextDestroy;
    std::atomic<int32_t> perfOpen;
    std::atomic<int32_t> perfEnable;
    std::atomic<int32_t> perfDisable;
};

class DrmMockSuccess : public Drm {
  public:
    using Drm::setupIoctlHelper;
    DrmMockSuccess(int fd, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(fd, mockPciPath), rootDeviceEnvironment) {

        DebugManagerStateRestore restore;
        debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
        setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
    }

    int ioctl(DrmIoctl request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {
        DebugManagerStateRestore restore;
        debugManager.flags.IgnoreProductSpecificIoctlHelper.set(true);
        setupIoctlHelper(NEO::defaultHwInfo->platform.eProductFamily);
    }

    int ioctl(DrmIoctl request, void *arg) override { return -1; };
};

class DrmMockTime : public DrmMockSuccess {
  public:
    using DrmMockSuccess::DrmMockSuccess;
    int ioctl(DrmIoctl request, void *arg) override {
        if (DrmIoctl::regRead == request) {
            auto *reg = reinterpret_cast<NEO::RegisterRead *>(arg);
            reg->value = getVal() << 32 | 0x1;
        }
        return 0;
    };

    uint64_t getVal() {
        static uint64_t val = 0;
        return ++val;
    }
};

struct DrmMockCustom : public Drm {
    using Drm::memoryInfoQueried;
    static constexpr NEO::DriverModelType driverModelType = NEO::DriverModelType::drm;

    static std::unique_ptr<DrmMockCustom> create(RootDeviceEnvironment &rootDeviceEnvironment);
    static std::unique_ptr<DrmMockCustom> create(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);

    using Drm::bindAvailable;
    using Drm::cacheInfo;
    using Drm::checkToDisableScratchPage;
    using Drm::completionFenceSupported;
    using Drm::disableScratch;
    using Drm::ioctlHelper;
    using Drm::memoryInfo;
    using Drm::pageFaultSupported;
    using Drm::queryTopology;
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
        ValueWidth dataWidth = ValueWidth::u8;
        int64_t timeout = 0;
        uint16_t flags = 0;

        uint32_t called = 0u;
        uint32_t failSpecificCall = 0;
        bool failOnWaitUserFence = false;
        int errnoForFailedWaitUserFence = 0;
    };

    struct IsVmBindAvailableCall {
        bool callParent = true;
        bool returnValue = true;
        uint32_t called = 0u;
    };

    struct IsChunkingAvailableCall {
        bool callParent = true;
        bool returnValue = true;
        uint32_t called = 0u;
    };

    struct ChunkingModeCall {
        bool callParent = true;
        uint32_t returnValue = 0x00;
        uint32_t called = 0u;
    };

    int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, NEO::GraphicsAllocation *allocForInterruptWait) override;

    bool getSetPairAvailable() override;

    bool getChunkingAvailable() override;
    uint32_t getChunkingMode() override;
    bool isChunkingAvailable() override;

    bool isVmBindAvailable() override;

    bool completionFenceSupport() override {
        return completionFenceSupported;
    }

    void testIoctls();

    int ioctl(DrmIoctl request, void *arg) override;

    void setFileDescriptor(int fd) {
        hwDeviceId = std::make_unique<HwDeviceIdDrm>(fd, "");
    }

    virtual int ioctlExtra(DrmIoctl request, void *arg) {
        return -1;
    }

    int getErrno() override {
        return errnoValue;
    }

    void reset() {
        ioctlRes = 0;
        ioctlCnt.reset();
        ioctlExpected.reset();
        ioctlResExt = &none;
    }

    virtual void execBufferExtensions(void *execbuf) {
    }
    int queryGttSize(uint64_t &gttSizeOutput, bool alignUpToFullRange) override {
        if (callBaseQueryGttSize) {
            return Drm::queryGttSize(gttSizeOutput, alignUpToFullRange);
        }
        gttSizeOutput = NEO::defaultHwInfo->capabilityTable.gpuAddressSpace + 1;
        return 0u;
    }

    bool checkResetStatus(OsContext &osContext) override {
        checkResetStatusCalled++;
        return Drm::checkResetStatus(osContext);
    }

    Ioctls ioctlCnt{};
    Ioctls ioctlExpected{};

    IoctlResExt none = {-1, 0};

    WaitUserFenceCall waitUserFenceCall{};
    IsVmBindAvailableCall getSetPairAvailableCall{};
    IsVmBindAvailableCall isVmBindAvailableCall{};

    IsChunkingAvailableCall getChunkingAvailableCall{};
    ChunkingModeCall getChunkingModeCall{};
    IsChunkingAvailableCall isChunkingAvailableCall{};

    size_t checkResetStatusCalled = 0u;

    std::atomic<int> ioctlRes;
    std::atomic<IoctlResExt *> ioctlResExt;

    // DRM_IOCTL_I915_GEM_EXECBUFFER2
    NEO::MockExecBuffer execBuffer{};

    // First exec object
    NEO::MockExecObject execBufferBufferObjects{};

    // DRM_IOCTL_I915_GEM_CREATE
    uint64_t createParamsSize = 0;
    uint32_t createParamsHandle = 0;
    // DRM_IOCTL_I915_GEM_SET_TILING
    uint32_t setTilingMode = 0;
    uint32_t setTilingHandle = 0;
    uint32_t setTilingStride = 0;
    // DRM_IOCTL_I915_GEM_GET_TILING
    uint32_t getTilingModeOut = 0;
    uint32_t getTilingHandleIn = 0;
    // DRM_IOCTL_PRIME_FD_TO_HANDLE
    uint32_t outputHandle = 0;
    int32_t inputFd = 0;
    // DRM_IOCTL_PRIME_HANDLE_TO_FD
    uint32_t inputHandle = 0;
    int32_t outputFd = 0;
    bool incrementOutputFdAfterCall = false;
    int32_t inputFlags = 0;
    // DRM_IOCTL_I915_GEM_USERPTR
    uint32_t returnHandle = 0;
    // DRM_IOCTL_I915_GEM_SET_DOMAIN
    uint32_t setDomainHandle = 0;
    uint32_t setDomainReadDomains = 0;
    uint32_t setDomainWriteDomain = 0;
    // DRM_IOCTL_I915_GETPARAM
    NEO::GetParam recordedGetParam = {0};
    int getParamRetValue = 0;
    // DRM_IOCTL_I915_GEM_CONTEXT_GETPARAM
    NEO::GemContextParam recordedGetContextParam = {0};
    uint64_t getContextParamRetValue = 0;
    // DRM_IOCTL_I915_GEM_WAIT
    int64_t gemWaitTimeout = 0;
    // DRM_IOCTL_I915_GEM_MMAP_OFFSET
    uint32_t mmapOffsetHandle = 0;
    uint32_t mmapOffsetPad = 0;
    uint64_t mmapOffsetExpected = 0;
    uint64_t mmapOffsetFlags = 0;
    bool failOnMmapOffset = false;
    bool failOnPrimeFdToHandle = false;
    bool failOnSecondPrimeFdToHandle = false;
    bool failOnPrimeHandleToFd = false;
    bool failOnSyncObjFdToHandle = false;
    bool failOnSyncObjWait = false;
    bool failOnSyncObjSignal = false;
    bool failOnSyncObjTimelineWait = false;
    bool failOnSyncObjTimelineSignal = false;

    // DRM_IOCTL_I915_GEM_CREATE_EXT
    uint64_t createExtSize = 0;
    uint32_t createExtHandle = 0;
    uint64_t createExtExtensions = 0;
    bool failOnCreateExt = false;

    uint32_t vmIdToCreate = 0;

    int errnoValue = 0;

    bool returnIoctlExtraErrorValue = false;
    bool callBaseQueryGttSize = false;

  protected:
    // Don't call directly, use the create() function
    DrmMockCustom(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
};
