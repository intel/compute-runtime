/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/os_interface/linux/device_command_stream_fixture.h"

#include <cstdio>
#include <fstream>
#include <limits.h>
#include <map>
#include <vector>

using namespace NEO;

class DrmMock : public Drm {
  public:
    using Drm::bindAvailable;
    using Drm::cacheInfo;
    using Drm::checkQueueSliceSupport;
    using Drm::classHandles;
    using Drm::completionFenceSupported;
    using Drm::contextDebugSupported;
    using Drm::engineInfo;
    using Drm::fenceVal;
    using Drm::generateElfUUID;
    using Drm::generateUUID;
    using Drm::getQueueSliceCount;
    using Drm::ioctlHelper;
    using Drm::memoryInfo;
    using Drm::nonPersistentContextsSupported;
    using Drm::pageFaultSupported;
    using Drm::pagingFence;
    using Drm::preemptionSupported;
    using Drm::query;
    using Drm::queryAndSetVmBindPatIndexProgrammingSupport;
    using Drm::queryDeviceIdAndRevision;
    using Drm::requirePerContextVM;
    using Drm::setPairAvailable;
    using Drm::setupIoctlHelper;
    using Drm::sliceCountChangeSupported;
    using Drm::systemInfo;
    using Drm::translateTopologyInfo;
    using Drm::virtualMemoryIds;

    DrmMock(int fd, RootDeviceEnvironment &rootDeviceEnvironment);
    DrmMock(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(mockFd, rootDeviceEnvironment) {}

    ~DrmMock() override;

    int ioctl(DrmIoctl request, void *arg) override;
    int getErrno() override {
        if (baseErrno) {
            return Drm::getErrno();
        }
        return errnoRetVal;
    }

    int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags) override;

    void writeConfigFile(const char *name, int deviceID) {
        std::ofstream tempfile(name, std::ios::binary);
        if (tempfile.is_open()) {
            PCIConfig config;
            config.DeviceID = deviceID;
            tempfile.write(reinterpret_cast<char *>(&config), sizeof(config));
            tempfile.close();
        }
    }

    void deleteConfigFile(const char *name) {
        std::ofstream tempfile(name);
        if (tempfile.is_open()) {
            tempfile.close();
            remove(name);
        }
    }

    void setFileDescriptor(int fd) {
        hwDeviceId = std::make_unique<HwDeviceIdDrm>(fd, "");
    }

    void setPciPath(const char *pciPath) {
        hwDeviceId = std::make_unique<HwDeviceIdDrm>(getFileDescriptor(), pciPath);
    }

    void setBindAvailable() {
        this->bindAvailable = true;
    }
    void setContextDebugFlag(uint32_t drmContextId) override {
        passedContextDebugId = drmContextId;
        return Drm::setContextDebugFlag(drmContextId);
    }

    bool isDebugAttachAvailable() override {
        if (allowDebugAttachCallBase) {
            return Drm::isDebugAttachAvailable();
        }
        return allowDebugAttach;
    }

    void queryPageFaultSupport() override {
        Drm::queryPageFaultSupport();
        queryPageFaultSupportCalled = true;
    }

    bool hasPageFaultSupport() const override {
        if (DebugManager.flags.EnableRecoverablePageFaults.get() != -1) {
            return !!DebugManager.flags.EnableRecoverablePageFaults.get();
        }

        return pageFaultSupported;
    }

    uint32_t createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested) override {
        capturedCooperativeContextRequest = isCooperativeContextRequested;
        if (callBaseCreateDrmContext) {
            return Drm::createDrmContext(drmVmId, isDirectSubmissionRequested, isCooperativeContextRequested);
        }

        return 0;
    }

    bool isVmBindAvailable() override {
        if (callBaseIsVmBindAvailable) {
            return Drm::isVmBindAvailable();
        }
        return bindAvailable;
    }

    bool isSetPairAvailable() override {
        if (callBaseIsSetPairAvailable) {
            return Drm::isSetPairAvailable();
        }
        return setPairAvailable;
    }

    bool getSetPairAvailable() override {
        if (callBaseGetSetPairAvailable) {
            return Drm::getSetPairAvailable();
        }
        return setPairAvailable;
    }

    uint32_t getBaseIoctlCalls() {
        return ioctlCallsForHelperInitialization + static_cast<uint32_t>(virtualMemoryIds.size());
    }

    static const int mockFd = 33;

    bool failRetTopology = false;
    bool baseErrno = true;
    int errnoRetVal = 0;
    int storedEUVal = 8;
    int storedSSVal = 2;
    int storedSVal = 1;
    int storedDeviceID = 1;
    int storedDeviceRevID = 1;
    int storedHasPooledEU = 1;
    int storedMinEUinPool = 1;
    int storedPersistentContextsSupport = 1;
    int storedRetVal = 0;
    int storedRetValForGetGttSize = 0;
    int storedRetValForGetSSEU = 0;
    int storedRetValForSetSSEU = 0;
    int storedRetValForDeviceID = 0;
    int storedRetValForEUVal = 0;
    int storedRetValForSSVal = 0;
    int storedRetValForDeviceRevID = 0;
    int storedRetValForPooledEU = 0;
    int storedRetValForMinEUinPool = 0;
    int storedRetValForPersistant = 0;
    int storedRetValForVmCreate = 0;
    int storedPreemptionSupport = 0;
    int storedExecSoftPin = 0;
    int storedRetValForVmId = 1;
    int storedCsTimestampFrequency = 1000;
    bool disableSomeTopology = false;
    bool allowDebugAttach = false;
    bool allowDebugAttachCallBase = false;
    bool callBaseCreateDrmContext = true;
    bool callBaseIsVmBindAvailable = false;
    bool callBaseIsSetPairAvailable = false;
    bool callBaseGetSetPairAvailable = false;

    bool capturedCooperativeContextRequest = false;
    bool incrementVmId = false;

    uint32_t passedContextDebugId = std::numeric_limits<uint32_t>::max();
    std::vector<ResetStats> resetStatsToReturn{};

    GemContextCreateExtSetParam receivedContextCreateSetParam = {};
    uint32_t receivedContextCreateFlags = 0;
    uint32_t receivedDestroyContextId = 0;
    uint32_t ioctlCallsCount = 0;

    uint32_t receivedContextParamRequestCount = 0;
    GemContextParam receivedContextParamRequest = {};
    uint64_t receivedRecoverableContextValue = std::numeric_limits<uint64_t>::max();

    bool queryPageFaultSupportCalled = false;

    //DRM_IOCTL_I915_GEM_EXECBUFFER2
    std::vector<MockExecBuffer> execBuffers{};
    std::vector<MockExecObject> receivedBos{};
    int execBufferResult = 0;
    //DRM_IOCTL_I915_GEM_CREATE
    uint64_t createParamsSize = 0;
    uint32_t createParamsHandle = 0;
    //DRM_IOCTL_I915_GEM_SET_TILING
    uint32_t setTilingMode = 0;
    uint32_t setTilingHandle = 0;
    uint32_t setTilingStride = 0;
    //DRM_IOCTL_PRIME_FD_TO_HANDLE
    uint32_t outputHandle = 0;
    int32_t inputFd = 0;
    int fdToHandleRetVal = 0;
    //DRM_IOCTL_HANDLE_TO_FD
    int32_t outputFd = 0;
    bool incrementOutputFdAfterCall = false;
    //DRM_IOCTL_I915_GEM_USERPTR
    uint32_t returnHandle = 0;
    uint64_t gpuMemSize = 3u * MemoryConstants::gigaByte;
    //DRM_IOCTL_I915_QUERY
    QueryItem storedQueryItem = {};
    //DRM_IOCTL_I915_GEM_WAIT
    GemWait receivedGemWait = {};
    //DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT
    uint32_t storedDrmContextId{};
    //DRM_IOCTL_GEM_CLOSE
    int storedRetValForGemClose = 0;

    GemVmControl receivedGemVmControl{};
    uint32_t latestCreatedVmId = 0u;

    uint64_t storedGTTSize = 1ull << 47;
    uint64_t storedParamSseu = ULONG_MAX;

    Ioctls ioctlCount{};
    Ioctls ioctlTearDownExpected{};
    bool ioctlTearDownExpects = false;

    bool expectIoctlCallsOnDestruction = false;
    uint32_t expectedIoctlCallsOnDestruction = 0u;

    virtual int handleRemainingRequests(DrmIoctl request, void *arg);

    struct WaitUserFenceParams {
        uint32_t ctxId;
        uint64_t address;
        uint64_t value;
        ValueWidth dataWidth;
        int64_t timeout;
        uint16_t flags;
    };
    StackVec<WaitUserFenceParams, 1> waitUserFenceParams;

    bool storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    bool useBaseGetDeviceMemoryMaxClockRateInMhz = true;
    bool getDeviceMemoryMaxClockRateInMhz(uint32_t tileId, uint32_t &clkRate) override {

        if (useBaseGetDeviceMemoryMaxClockRateInMhz == true) {
            return Drm::getDeviceMemoryMaxClockRateInMhz(tileId, clkRate);
        }

        if (storedGetDeviceMemoryMaxClockRateInMhzStatus == true) {
            clkRate = 800;
        }
        return storedGetDeviceMemoryMaxClockRateInMhzStatus;
    }

    bool storedGetDeviceMemoryPhysicalSizeInBytesStatus = true;
    bool useBaseGetDeviceMemoryPhysicalSizeInBytes = true;
    bool getDeviceMemoryPhysicalSizeInBytes(uint32_t tileId, uint64_t &physicalSize) override {
        if (useBaseGetDeviceMemoryPhysicalSizeInBytes == true) {
            return Drm::getDeviceMemoryPhysicalSizeInBytes(tileId, physicalSize);
        }

        if (storedGetDeviceMemoryPhysicalSizeInBytesStatus == true) {
            physicalSize = 1024;
        }
        return storedGetDeviceMemoryPhysicalSizeInBytesStatus;
    }
    static uint32_t ioctlCallsForHelperInitialization;
};

class DrmMockNonFailing : public DrmMock {
  public:
    using DrmMock::DrmMock;
    int handleRemainingRequests(DrmIoctl request, void *arg) override { return 0; }
};

class DrmMockReturnErrorNotSupported : public DrmMock {
  public:
    using DrmMock::DrmMock;
    int ioctl(DrmIoctl request, void *arg) override {
        if (request == DrmIoctl::GemExecbuffer2) {
            return -1;
        }
        return 0;
    }
    int getErrno() override { return EOPNOTSUPP; }
};

class DrmMockEngine : public DrmMock {
  public:
    uint32_t i915QuerySuccessCount = std::numeric_limits<uint32_t>::max();
    uint32_t queryEngineInfoSuccessCount = std::numeric_limits<uint32_t>::max();

    DrmMockEngine(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(rootDeviceEnvironment) {
        rootDeviceEnvironment.setHwInfo(defaultHwInfo.get());
    }

    int handleRemainingRequests(DrmIoctl request, void *arg) override;

    void handleQueryItem(QueryItem *queryItem);
    bool failQueryDeviceBlob = false;
};

class DrmMockResources : public DrmMock {
  public:
    DrmMockResources(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMock(mockFd, rootDeviceEnvironment) {
        setBindAvailable();
        callBaseIsVmBindAvailable = true;
    }

    bool registerResourceClasses() override {
        registerClassesCalled = true;
        return true;
    }

    uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size) override {
        registeredClass = classType;
        memcpy_s(registeredData, sizeof(registeredData), data, size);
        registeredDataSize = size;
        return registerResourceReturnHandle;
    }

    void unregisterResource(uint32_t handle) override {
        unregisterCalledCount++;
        unregisteredHandle = handle;
    }

    uint32_t registerIsaCookie(uint32_t isaHanlde) override {
        return currentCookie++;
    }

    bool isVmBindAvailable() override {
        return bindAvailable;
    }

    uint32_t notifyFirstCommandQueueCreated(const void *data, size_t size) override {
        ioctlCallsCount++;
        capturedCmdQData = std::make_unique<uint64_t[]>((size + sizeof(uint64_t) - 1) / sizeof(uint64_t));
        capturedCmdQSize = size;
        memcpy(capturedCmdQData.get(), data, size);
        return 4;
    }

    void notifyLastCommandQueueDestroyed(uint32_t handle) override {
        unregisterResource(handle);
    }

    static const uint32_t registerResourceReturnHandle;

    uint32_t unregisteredHandle = 0;
    uint32_t unregisterCalledCount = 0;
    DrmResourceClass registeredClass = DrmResourceClass::MaxSize;
    bool registerClassesCalled = false;
    uint64_t registeredData[128];
    size_t registeredDataSize;
    uint32_t currentCookie = 2;
    std::unique_ptr<uint64_t[]> capturedCmdQData = nullptr;
    size_t capturedCmdQSize = 0;
};
