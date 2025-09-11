/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/queue_throttle.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/utilities/cpuintrinsics.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>

namespace NEO {
class MemoryManager;
struct RootDeviceEnvironment;

#pragma pack(1)
struct RingSemaphoreData {
    uint32_t queueWorkCount;
    uint8_t reservedCacheline0[60];
    uint32_t tagAllocation;
    uint8_t reservedCacheline1[60];
    uint8_t reservedCacheline2[64];
    uint64_t miFlushSpace;
    uint8_t reservedCacheline3[56];
    uint32_t pagingFenceCounter;
    uint8_t reservedCacheline4[60];
};
static_assert((64u * 5) == sizeof(RingSemaphoreData), "Invalid size for RingSemaphoreData");
#pragma pack()

using DirectSubmissionAllocations = StackVec<GraphicsAllocation *, 8>;

struct TagData {
    uint64_t tagAddress = 0ull;
    uint64_t tagValue = 0ull;
};

struct BatchBuffer;
class FlushStampTracker;
class GraphicsAllocation;
struct HardwareInfo;
class OsContext;
class MemoryOperationsHandler;

struct DirectSubmissionInputParams : NonCopyableClass {
    DirectSubmissionInputParams(const CommandStreamReceiver &commandStreamReceiver);
    OsContext &osContext;
    const RootDeviceEnvironment &rootDeviceEnvironment;
    MemoryManager *memoryManager = nullptr;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    GraphicsAllocation *completionFenceAllocation = nullptr;
    TaskCountType initialCompletionFenceValue = 0;
    const uint32_t rootDeviceIndex;
};

template <typename GfxFamily, typename Dispatcher>
class DirectSubmissionHw {
  public:
    DirectSubmissionHw(const DirectSubmissionInputParams &inputParams);

    virtual ~DirectSubmissionHw();

    bool initialize(bool submitOnInit);

    MOCKABLE_VIRTUAL bool stopRingBuffer(bool blocking);

    MOCKABLE_VIRTUAL bool dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp);
    uint32_t getDispatchErrorCode();

    static std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> create(const DirectSubmissionInputParams &inputParams);

    virtual TaskCountType *getCompletionValuePointer() { return nullptr; }

    bool isRelaxedOrderingEnabled() const {
        return relaxedOrderingEnabled;
    }

    virtual void flushMonitorFence(bool notifyKmd){};

    QueueThrottle getLastSubmittedThrottle() {
        return this->lastSubmittedThrottle;
    }

    virtual void unblockPagingFenceSemaphore(uint64_t pagingFenceValue){};
    uint32_t getRelaxedOrderingQueueSize() const { return currentRelaxedOrderingQueueSize; }

  protected:
    struct SemaphoreFenceHelper : public NonCopyableAndNonMovableClass {
        SemaphoreFenceHelper(const auto &directSubmission) : directSubmission(directSubmission) {
            if (!directSubmission.miMemFenceRequired && !directSubmission.pciBarrierPtr && !directSubmission.hwInfo->capabilityTable.isIntegratedDevice) {
                CpuIntrinsics::mfence();
            } else {
                CpuIntrinsics::sfence();
            }
        }
        ~SemaphoreFenceHelper() {
            CpuIntrinsics::sfence();
        }

        const DirectSubmissionHw<GfxFamily, Dispatcher> &directSubmission;
    };

    static_assert(NEO::NonCopyableAndNonMovable<SemaphoreFenceHelper>);

    static constexpr size_t prefetchSize = 8 * MemoryConstants::cacheLineSize;
    static constexpr size_t prefetchNoops = prefetchSize / sizeof(uint32_t);
    bool allocateResources();
    MOCKABLE_VIRTUAL void deallocateResources();
    MOCKABLE_VIRTUAL bool makeResourcesResident(DirectSubmissionAllocations &allocations);
    virtual bool allocateOsResources();
    virtual bool submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) = 0;
    virtual bool handleResidency() = 0;
    virtual void handleRingRestartForUllsLightResidency(const ResidencyContainer *allocationsForResidency){};
    virtual void handleResidencyContainerForUllsLightNewRingAllocation(ResidencyContainer *allocationsForResidency){};
    void handleNewResourcesSubmission();
    bool isNewResourceHandleNeeded();
    size_t getSizeNewResourceHandler();
    virtual void handleStopRingBuffer(){};
    virtual void ensureRingCompletion(){};
    void switchRingBuffersNeeded(size_t size, ResidencyContainer *allocationsForResidency);
    uint64_t switchRingBuffers(ResidencyContainer *allocationsForResidency);
    virtual void handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) = 0;
    GraphicsAllocation *switchRingBuffersAllocations(ResidencyContainer *allocationsForResidency);

    constexpr static uint64_t updateTagValueFail = std::numeric_limits<uint64_t>::max();
    virtual uint64_t updateTagValue(bool requireMonitorFence) = 0;
    virtual bool dispatchMonitorFenceRequired(bool requireMonitorFence);
    virtual void getTagAddressValue(TagData &tagData) = 0;
    virtual void getTagAddressValueForRingSwitch(TagData &tagData) = 0;
    void unblockGpu();
    bool submitCommandBufferToGpu(bool needStart, uint64_t gpuAddress, size_t size, bool needWait, const ResidencyContainer *allocationsForResidency);
    bool copyCommandBufferIntoRing(BatchBuffer &batchBuffer);

    void dispatchSemaphoreSection(uint32_t value);
    size_t getSizeSemaphoreSection(bool relaxedOrderingSchedulerRequired);

    void dispatchSemaphoreForPagingFence(uint64_t value);
    size_t getSizeSemaphoreForPagingFence();

    MOCKABLE_VIRTUAL void dispatchRelaxedOrderingSchedulerSection(uint32_t value);

    void dispatchRelaxedOrderingReturnPtrRegs(LinearStream &cmdStream, uint64_t returnPtr);

    void dispatchStartSection(uint64_t gpuStartAddress);
    size_t getSizeStartSection();

    size_t getUllsStateSize();
    void dispatchUllsState();

    void dispatchSwitchRingBufferSection(uint64_t nextBufferGpuAddress);
    size_t getSizeSwitchRingBufferSection();

    MOCKABLE_VIRTUAL void dispatchRelaxedOrderingQueueStall();
    size_t getSizeDispatchRelaxedOrderingQueueStall();

    MOCKABLE_VIRTUAL void dispatchTaskStoreSection(uint64_t taskStartSectionVa);
    MOCKABLE_VIRTUAL void preinitializeRelaxedOrderingSections();

    void initRelaxedOrderingRegisters();

    void setReturnAddress(void *returnCmd, uint64_t returnAddress);

    void *dispatchWorkloadSection(BatchBuffer &batchBuffer, bool dispatchMonitorFence);
    size_t getSizeDispatch(bool relaxedOrderingSchedulerRequired, bool returnPtrsRequired, bool dispatchMonitorFence);

    void dispatchPrefetchMitigation();
    size_t getSizePrefetchMitigation();

    void dispatchDisablePrefetcher(bool disable);
    size_t getSizeDisablePrefetcher();

    MOCKABLE_VIRTUAL void dispatchStaticRelaxedOrderingScheduler();

    size_t getSizeEnd(bool relaxedOrderingSchedulerRequired);

    void dispatchPartitionRegisterConfiguration();
    size_t getSizePartitionRegisterConfigurationSection();

    void dispatchSystemMemoryFenceAddress();
    size_t getSizeSystemMemoryFenceAddress();

    void setImmWritePostSyncOffset();
    virtual void dispatchStopRingBufferSection(){};
    virtual size_t dispatchStopRingBufferSectionSize() {
        return 0;
    };

    virtual bool isCompleted(uint32_t ringBufferIndex) = 0;

    void updateRelaxedOrderingQueueSize(uint32_t newSize);

    virtual void makeGlobalFenceAlwaysResident(){};
    struct RingBufferUse {
        RingBufferUse() = default;
        RingBufferUse(FlushStamp completionFence, FlushStamp completionFenceForSwitch, GraphicsAllocation *ringBuffer) : completionFence(completionFence), completionFenceForSwitch(completionFenceForSwitch), ringBuffer(ringBuffer){};

        constexpr static uint32_t initialRingBufferCount = 2u;

        FlushStamp completionFence = 0ull;
        FlushStamp completionFenceForSwitch = 0ull;
        GraphicsAllocation *ringBuffer = nullptr;
    };
    std::vector<RingBufferUse> ringBuffers;
    std::unique_ptr<uint8_t[]> preinitializedTaskStoreSection;
    std::unique_ptr<uint8_t[]> preinitializedRelaxedOrderingScheduler;
    uint32_t currentRingBuffer = 0u;
    uint32_t previousRingBuffer = 0u;
    uint32_t maxRingBufferCount = std::numeric_limits<uint32_t>::max();

    LinearStream ringCommandStream;

    uint64_t semaphoreGpuVa = 0u;
    uint64_t gpuVaForMiFlush = 0u;
    uint64_t gpuVaForAdditionalSynchronizationWA = 0u;
    uint64_t gpuVaForPagingFenceSemaphore = 0u;
    uint64_t relaxedOrderingQueueSizeLimitValueVa = 0;

    OsContext &osContext;
    const uint32_t rootDeviceIndex;
    MemoryManager *memoryManager = nullptr;
    MemoryOperationsHandler *memoryOperationHandler = nullptr;
    const HardwareInfo *hwInfo = nullptr;
    const RootDeviceEnvironment &rootDeviceEnvironment;
    GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *completionFenceAllocation = nullptr;
    GraphicsAllocation *semaphores = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    GraphicsAllocation *deferredTasksListAllocation = nullptr;
    GraphicsAllocation *relaxedOrderingSchedulerAllocation = nullptr;
    void *semaphorePtr = nullptr;
    volatile RingSemaphoreData *semaphoreData = nullptr;
    uint32_t *pciBarrierPtr = nullptr;
    volatile TagAddressType *tagAddress = nullptr;

    uint32_t currentQueueWorkCount = 1u;
    uint32_t activeTiles = 1u;
    uint32_t immWritePostSyncOffset = 0u;
    uint32_t currentRelaxedOrderingQueueSize = 0;
    volatile uint32_t reserved = 0u;
    uint32_t dispatchErrorCode = 0;
    QueueThrottle lastSubmittedThrottle = QueueThrottle::MEDIUM;

    bool ringStart = false;
    bool partitionedMode = false;
    bool partitionConfigSet = true;
    bool miMemFenceRequired = false;
    bool systemMemoryFenceAddressSet = false;
    bool completionFenceSupported = false;
    bool isDisablePrefetcherRequired = false;
    bool dcFlushRequired = false;
    bool detectGpuHang = true;
    bool relaxedOrderingEnabled = false;
    bool relaxedOrderingInitialized = false;
    bool relaxedOrderingSchedulerRequired = false;
    bool notifyKmdDuringMonitorFence = false;
};
} // namespace NEO
