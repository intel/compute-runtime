/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>

namespace NEO {

#pragma pack(1)
struct RingSemaphoreData {
    uint32_t QueueWorkCount;
    uint8_t ReservedCacheline0[60];
    uint32_t tagAllocation;
    uint8_t ReservedCacheline1[60];
    uint32_t DiagnosticModeCounter;
    uint32_t Reserved0Uint32;
    uint64_t Reserved1Uint64;
    uint8_t ReservedCacheline2[48];
    uint64_t miFlushSpace;
    uint8_t ReservedCacheline3[56];
};
static_assert((64u * 4) == sizeof(RingSemaphoreData), "Invalid size for RingSemaphoreData");
#pragma pack()

using DirectSubmissionAllocations = StackVec<GraphicsAllocation *, 8>;

struct TagData {
    uint64_t tagAddress = 0ull;
    uint64_t tagValue = 0ull;
};

enum class DirectSubmissionSfenceMode : int32_t {
    Disabled = 0,
    BeforeSemaphoreOnly = 1,
    BeforeAndAfterSemaphore = 2
};

namespace UllsDefaults {
constexpr bool defaultDisableCacheFlush = true;
constexpr bool defaultDisableMonitorFence = false;
} // namespace UllsDefaults

struct BatchBuffer;
class DirectSubmissionDiagnosticsCollector;
class FlushStampTracker;
class GraphicsAllocation;
class LogicalStateHelper;
struct HardwareInfo;
class OsContext;
class MemoryOperationsHandler;

struct DirectSubmissionInputParams : NonCopyableClass {
    DirectSubmissionInputParams(const CommandStreamReceiver &commandStreamReceiver);
    OsContext &osContext;
    const RootDeviceEnvironment &rootDeviceEnvironment;
    LogicalStateHelper *logicalStateHelper = nullptr;
    MemoryManager *memoryManager = nullptr;
    const GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    GraphicsAllocation *completionFenceAllocation = nullptr;
    const uint32_t rootDeviceIndex;
};

template <typename GfxFamily, typename Dispatcher>
class DirectSubmissionHw {
  public:
    DirectSubmissionHw(const DirectSubmissionInputParams &inputParams);

    virtual ~DirectSubmissionHw();

    bool initialize(bool submitOnInit, bool useNotify);

    MOCKABLE_VIRTUAL bool stopRingBuffer();

    bool startRingBuffer();

    MOCKABLE_VIRTUAL bool dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp);

    static std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> create(const DirectSubmissionInputParams &inputParams);

    virtual uint32_t *getCompletionValuePointer() { return nullptr; }

  protected:
    static constexpr size_t prefetchSize = 8 * MemoryConstants::cacheLineSize;
    static constexpr size_t prefetchNoops = prefetchSize / sizeof(uint32_t);
    bool allocateResources();
    MOCKABLE_VIRTUAL void deallocateResources();
    MOCKABLE_VIRTUAL bool makeResourcesResident(DirectSubmissionAllocations &allocations);
    virtual bool allocateOsResources() = 0;
    virtual bool submit(uint64_t gpuAddress, size_t size) = 0;
    virtual bool handleResidency() = 0;
    virtual void handleNewResourcesSubmission();
    virtual size_t getSizeNewResourceHandler();
    virtual void handleStopRingBuffer(){};
    virtual uint64_t switchRingBuffers();
    virtual void handleSwitchRingBuffers() = 0;
    GraphicsAllocation *switchRingBuffersAllocations();
    virtual uint64_t updateTagValue() = 0;
    virtual void getTagAddressValue(TagData &tagData) = 0;

    void cpuCachelineFlush(void *ptr, size_t size);

    void dispatchSemaphoreSection(uint32_t value);
    size_t getSizeSemaphoreSection();

    void dispatchStartSection(uint64_t gpuStartAddress);
    size_t getSizeStartSection();

    void dispatchSwitchRingBufferSection(uint64_t nextBufferGpuAddress);
    size_t getSizeSwitchRingBufferSection();

    void setReturnAddress(void *returnCmd, uint64_t returnAddress);

    void *dispatchWorkloadSection(BatchBuffer &batchBuffer);
    size_t getSizeDispatch();

    void dispatchPrefetchMitigation();
    size_t getSizePrefetchMitigation();

    void dispatchDisablePrefetcher(bool disable);
    size_t getSizeDisablePrefetcher();

    size_t getSizeEnd();

    uint64_t getCommandBufferPositionGpuAddress(void *position);

    void dispatchPartitionRegisterConfiguration();
    size_t getSizePartitionRegisterConfigurationSection();

    void dispatchSystemMemoryFenceAddress();
    size_t getSizeSystemMemoryFenceAddress();

    void createDiagnostic();
    void initDiagnostic(bool &submitOnInit);
    MOCKABLE_VIRTUAL void performDiagnosticMode();
    void dispatchDiagnosticModeSection();
    size_t getDiagnosticModeSection();
    void setPostSyncOffset();

    virtual bool isCompleted(uint32_t ringBufferIndex) = 0;

    struct RingBufferUse {
        RingBufferUse() = default;
        RingBufferUse(FlushStamp completionFence, GraphicsAllocation *ringBuffer) : completionFence(completionFence), ringBuffer(ringBuffer){};

        constexpr static uint32_t initialRingBufferCount = 2u;

        FlushStamp completionFence = 0ull;
        GraphicsAllocation *ringBuffer = nullptr;
    };
    std::vector<RingBufferUse> ringBuffers;
    uint32_t currentRingBuffer = 0u;
    uint32_t previousRingBuffer = 0u;
    uint32_t maxRingBufferCount = std::numeric_limits<uint32_t>::max();

    LinearStream ringCommandStream;
    std::unique_ptr<DirectSubmissionDiagnosticsCollector> diagnostic;

    uint64_t semaphoreGpuVa = 0u;
    uint64_t gpuVaForMiFlush = 0u;
    uint64_t gpuVaForAdditionalSynchronizationWA = 0u;

    OsContext &osContext;
    const uint32_t rootDeviceIndex;
    MemoryManager *memoryManager = nullptr;
    LogicalStateHelper *logicalStateHelper = nullptr;
    MemoryOperationsHandler *memoryOperationHandler = nullptr;
    const HardwareInfo *hwInfo = nullptr;
    const GraphicsAllocation *globalFenceAllocation = nullptr;
    GraphicsAllocation *completionFenceAllocation = nullptr;
    GraphicsAllocation *semaphores = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    void *semaphorePtr = nullptr;
    volatile RingSemaphoreData *semaphoreData = nullptr;
    volatile void *workloadModeOneStoreAddress = nullptr;

    uint32_t currentQueueWorkCount = 1u;
    uint32_t workloadMode = 0;
    uint32_t workloadModeOneExpectedValue = 0u;
    uint32_t activeTiles = 1u;
    uint32_t postSyncOffset = 0u;
    DirectSubmissionSfenceMode sfenceMode = DirectSubmissionSfenceMode::BeforeAndAfterSemaphore;
    volatile uint32_t reserved = 0u;

    bool ringStart = false;
    bool disableCpuCacheFlush = true;
    bool disableCacheFlush = false;
    bool disableMonitorFence = false;
    bool partitionedMode = false;
    bool partitionConfigSet = true;
    bool useNotifyForPostSync = false;
    bool miMemFenceRequired = false;
    bool systemMemoryFenceAddressSet = false;
    bool completionFenceSupported = false;
    bool isDisablePrefetcherRequired = false;
};
} // namespace NEO
