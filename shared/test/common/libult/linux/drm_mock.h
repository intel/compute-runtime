/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/linux/mock_drm_wrappers.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <climits>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <vector>

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
    std::atomic<int32_t> syncObjDestroy;
    std::atomic<int32_t> syncObjWait;
    std::atomic<int32_t> syncObjSignal;
    std::atomic<int32_t> syncObjTimelineWait;
    std::atomic<int32_t> syncObjTimelineSignal;
    std::atomic<int32_t> gemMmapOffset;
    std::atomic<int32_t> gemSetDomain;
    std::atomic<int32_t> gemWait;
    std::atomic<int32_t> gemClose;
    std::atomic<int32_t> queryContextHealth;
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

using namespace NEO;

class DrmMockSuccess : public Drm {
  public:
    using Drm::setupIoctlHelper;
    DrmMockSuccess(int fd, RootDeviceEnvironment &rootDeviceEnvironment);

    int ioctl(DrmIoctl request, void *arg) override { return 0; };
};

class DrmMockFail : public Drm {
  public:
    DrmMockFail(RootDeviceEnvironment &rootDeviceEnvironment);

    int ioctl(DrmIoctl request, void *arg) override { return -1; };
};

class DrmMockTime : public DrmMockSuccess {
  public:
    using Drm::ioctlHelper;
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

struct MockResetStats {
    uint32_t contextId;
    uint32_t batchActive;
    uint32_t batchPending;
};

class DrmMock : public Drm {
  public:
    using BaseClass = Drm;
    using Drm::adapterBDF;
    using Drm::bindAddDebugData;
    using Drm::bindAvailable;
    using Drm::cacheInfo;
    using Drm::checkQueueSliceSupport;
    using Drm::chunkingAvailable;
    using Drm::chunkingMode;
    using Drm::completionFenceSupported;
    using Drm::contextDebugSupported;
    using Drm::disableScratch;
    using Drm::drmFabric;
    using Drm::engineInfo;
    using Drm::engineInfoQueried;
    using Drm::fenceVal;
    using Drm::generateElfUUID;
    using Drm::generateUUID;
    using Drm::getQueueSliceCount;
    using Drm::getSharedSystemAllocAddressRange;
    using Drm::ioctlHelper;
    using Drm::isSharedSystemAllocEnabled;
    using Drm::memoryInfo;
    using Drm::memoryInfoQueried;
    using Drm::minimalChunkingSize;
    using Drm::nonPersistentContextsSupported;
    using Drm::pageFaultSupported;
    using Drm::pagingFence;
    using Drm::preemptionSupported;
    using Drm::query;
    using Drm::queryAndSetVmBindPatIndexProgrammingSupport;
    using Drm::queryDeviceIdAndRevision;
    using Drm::requirePerContextVM;
    using Drm::setPageFaultSupported;
    using Drm::setPairAvailable;
    using Drm::setSharedSystemAllocEnable;
    using Drm::setupDrmFabric;
    using Drm::setupIoctlHelper;
    using Drm::sliceCountChangeSupported;
    using Drm::systemInfo;
    using Drm::systemInfoQueried;
    using Drm::topologyQueried;
    using Drm::virtualMemoryIds;
    using Drm::vmBindPatIndexProgrammingSupported;

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

    int waitUserFence(uint32_t ctxId, uint64_t address, uint64_t value, ValueWidth dataWidth, int64_t timeout, uint16_t flags, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override;

    std::string getSysFsPciPath() override {
        if (!mockSysFsPciPath.empty()) {
            return mockSysFsPciPath;
        }
        return Drm::getSysFsPciPath();
    }
    std::string mockSysFsPciPath{};

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

    int createDrmContext(uint32_t drmVmId, bool isDirectSubmissionRequested, bool isCooperativeContextRequested) override {
        capturedCooperativeContextRequest = isCooperativeContextRequested;
        if (callBaseCreateDrmContext) {
            return Drm::createDrmContext(drmVmId, isDirectSubmissionRequested, isCooperativeContextRequested);
        }

        return 0;
    }

    int createDrmVirtualMemory(uint32_t &drmVmId) override {
        createDrmVmCalled++;
        return Drm::createDrmVirtualMemory(drmVmId);
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

    bool isChunkingAvailable() override {
        if (callBaseIsChunkingAvailable) {
            return Drm::isChunkingAvailable();
        }
        return chunkingAvailable;
    }

    uint32_t getChunkingMode() override {
        if (callBaseChunkingMode) {
            return Drm::isChunkingAvailable();
        }
        return chunkingMode;
    }

    bool getSetPairAvailable() override {
        if (callBaseGetSetPairAvailable) {
            return Drm::getSetPairAvailable();
        }
        return setPairAvailable;
    }

    uint32_t getBaseIoctlCalls() {
        return static_cast<uint32_t>(virtualMemoryIds.size());
    }
    bool useVMBindImmediate() const override {
        if (isVMBindImmediateSupported.has_value()) {
            return *isVMBindImmediateSupported;
        } else {
            return Drm::useVMBindImmediate();
        }
    }
    int queryGttSize(uint64_t &gttSizeOutput, bool alignUpToFullRange) override {
        gttSizeOutput = storedGTTSize;
        return storedRetValForGetGttSize;
    }

    uint32_t getAggregatedProcessCount() const override {
        return mockProcessCount;
    }

    ADDMETHOD_CONST(getDriverModelType, DriverModelType, true, DriverModelType::drm, (), ());

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
    int storedRetValForVmId = 1;
    int storedCsTimestampFrequency = 1000;
    int storedOaTimestampFrequency = 123456;
    int storedRetValForHasContextFreqHint = 1;
    uint32_t mockProcessCount = 1;
    bool disableSomeTopology = false;
    bool allowDebugAttach = false;
    bool allowDebugAttachCallBase = false;
    bool callBaseCreateDrmContext = true;
    bool callBaseIsVmBindAvailable = false;
    bool callBaseIsSetPairAvailable = false;
    bool callBaseIsChunkingAvailable = false;
    bool callBaseGetChunkingMode = false;
    bool callBaseChunkingMode = false;
    bool callBaseGetSetPairAvailable = false;
    bool unrecoverableContextSet = false;
    bool failRetHwIpVersion = false;
    bool returnInvalidHwIpVersionLength = false;
    bool failPerfOpen = false;
    DeviceDescriptor *overrideDeviceDescriptor = nullptr;

    bool capturedCooperativeContextRequest = false;
    bool incrementVmId = false;
    bool allowMissingResetStats = false;
    std::optional<bool> isVMBindImmediateSupported{};

    uint32_t passedContextDebugId = std::numeric_limits<uint32_t>::max();
    std::vector<MockResetStats> resetStatsToReturn{};

    GemContextCreateExtSetParam receivedContextCreateSetParam = {};
    uint32_t receivedContextCreateFlags = 0;
    uint32_t receivedDestroyContextId = 0;
    uint32_t ioctlCallsCount = 0;

    uint32_t receivedContextParamRequestCount = 0;
    GemContextParam receivedContextParamRequest = {};
    uint64_t receivedRecoverableContextValue = std::numeric_limits<uint64_t>::max();
    uint64_t requestSetVmId = std::numeric_limits<uint64_t>::max();
    int createDrmVmCalled = 0;

    bool queryPageFaultSupportCalled = false;

    // DRM_IOCTL_I915_GEM_EXECBUFFER2
    std::vector<MockExecBuffer> execBuffers{};
    std::vector<MockExecObject> receivedBos{};
    int execBufferResult = 0;
    // DRM_IOCTL_I915_GEM_CREATE
    uint64_t createParamsSize = 0;
    uint32_t createParamsHandle = 0;
    // DRM_IOCTL_I915_GEM_SET_TILING
    uint32_t setTilingMode = 0;
    uint32_t setTilingHandle = 0;
    uint32_t setTilingStride = 0;
    // DRM_IOCTL_PRIME_FD_TO_HANDLE
    uint32_t outputHandle = 0;
    int32_t inputFd = 0;
    int fdToHandleRetVal = 0;
    // DRM_IOCTL_HANDLE_TO_FD
    int32_t outputFd = 0;
    bool incrementOutputFdAfterCall = false;
    // DRM_IOCTL_I915_GEM_USERPTR
    uint32_t returnHandle = 0;
    uint64_t gpuMemSize = 3u * MemoryConstants::gigaByte;
    // DRM_IOCTL_I915_QUERY
    QueryItem storedQueryItem = {};
    // DRM_IOCTL_I915_GEM_WAIT
    GemWait receivedGemWait = {};
    // DRM_IOCTL_I915_GEM_CONTEXT_CREATE_EXT
    uint32_t storedDrmContextId{};
    // DRM_IOCTL_GEM_CLOSE
    int storedRetValForGemClose = 0;

    GemVmControl receivedGemVmControl{};
    uint32_t latestCreatedVmId = 0u;

    uint64_t storedGTTSize = defaultHwInfo->capabilityTable.gpuAddressSpace + 1;
    uint64_t storedParamSseu = ULONG_MAX;

    Ioctls ioctlCount{};
    Ioctls ioctlTearDownExpected{};
    bool ioctlTearDownExpects = false;

    bool expectIoctlCallsOnDestruction = false;
    uint32_t expectedIoctlCallsOnDestruction = 0u;
    bool lowLatencyHintRequested = false;

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
    StackVec<GemVmBind, 1> vmBindInputs;

    bool storedGetDeviceMemoryMaxClockRateInMhzStatus = true;
    bool useBaseGetDeviceMemoryMaxClockRateInMhz = true;
    uint32_t getDeviceMemoryMaxClockRateInMhz(uint32_t tileId) override {

        if (useBaseGetDeviceMemoryMaxClockRateInMhz == true) {
            return Drm::getDeviceMemoryMaxClockRateInMhz(tileId);
        }

        return storedGetDeviceMemoryMaxClockRateInMhzStatus ? 800u : 0u;
    }

    bool storedGetDeviceMemoryPhysicalSizeInBytesStatus = true;
    bool useBaseGetDeviceMemoryPhysicalSizeInBytes = true;
    uint64_t getDeviceMemoryPhysicalSizeInBytes(uint32_t tileId) override {
        if (useBaseGetDeviceMemoryPhysicalSizeInBytes == true) {
            return Drm::getDeviceMemoryPhysicalSizeInBytes(tileId);
        }

        return storedGetDeviceMemoryPhysicalSizeInBytesStatus ? 1024u : 0u;
    }

    const DeviceDescriptor *getDeviceDescriptor(uint32_t usDeviceId) override {
        if (overrideDeviceDescriptor) {
            return overrideDeviceDescriptor;
        }
        return Drm::getDeviceDescriptor(usDeviceId);
    }

    [[nodiscard]] std::unique_lock<std::mutex> lockBindFenceMutex() override {
        lockBindFenceMutexCallCount++;
        return Drm::lockBindFenceMutex();
    }
    uint32_t lockBindFenceMutexCallCount = 0u;
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
        if (request == DrmIoctl::gemExecbuffer2) {
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

    DrmMockEngine(RootDeviceEnvironment &rootDeviceEnvironment);

    int handleRemainingRequests(DrmIoctl request, void *arg) override;

    void handleQueryItem(QueryItem *queryItem);
    void adjustL3BankGroupsInfo(QueryItem *queryItem);
    bool failQueryDeviceBlob = false;
    bool dontQueryL3BankGroups = false;
    bool dontQueryL3BanksPerGroup = false;
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
        notifyFirstCommandQueueCreatedCallsCount++;
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
    uint32_t notifyFirstCommandQueueCreatedCallsCount = 0;
    DrmResourceClass registeredClass = DrmResourceClass::maxSize;
    bool registerClassesCalled = false;
    uint64_t registeredData[128];
    size_t registeredDataSize;
    uint32_t currentCookie = 2;
    std::unique_ptr<uint64_t[]> capturedCmdQData = nullptr;
    size_t capturedCmdQSize = 0;
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
    // DRM_IOCTL_SYNCOBJ_WAIT
    int64_t syncObjWaitTimeoutNs = 0;
    uint32_t syncObjWaitFlags = 0;
    uint32_t syncObjWaitCountHandles = 0;
    // DRM_IOCTL_SYNCOBJ_SIGNAL
    uint32_t syncObjSignalCountHandles = 0;
    // DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT
    int64_t syncObjTimelineWaitTimeoutNs = 0;
    uint32_t syncObjTimelineWaitFlags = 0;
    std::vector<uint64_t> syncObjTimelineWaitPoints;
    // DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL
    std::vector<uint64_t> syncObjTimelineSignalPoints;

    bool failOnMmapOffset = false;
    bool failOnPrimeFdToHandle = false;
    bool failOnSecondPrimeFdToHandle = false;
    bool failOnPrimeHandleToFd = false;
    bool failOnSyncObjFdToHandle = false;
    bool failOnSyncObjDestroy = false;
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
