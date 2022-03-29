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

namespace UllsDefaults {
constexpr bool defaultDisableCacheFlush = true;
constexpr bool defaultDisableMonitorFence = false;
} // namespace UllsDefaults

struct BatchBuffer;
class DirectSubmissionDiagnosticsCollector;
class FlushStampTracker;
class GraphicsAllocation;
struct HardwareInfo;
class Device;
class OsContext;

template <typename GfxFamily, typename Dispatcher>
class DirectSubmissionHw {
  public:
    DirectSubmissionHw(Device &device,
                       OsContext &osContext);

    virtual ~DirectSubmissionHw();

    bool initialize(bool submitOnInit, bool useNotify);

    MOCKABLE_VIRTUAL bool stopRingBuffer();

    bool startRingBuffer();

    MOCKABLE_VIRTUAL bool dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp);

    static std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> create(Device &device, OsContext &osContext);

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

    void createDiagnostic();
    void initDiagnostic(bool &submitOnInit);
    MOCKABLE_VIRTUAL void performDiagnosticMode();
    void dispatchDiagnosticModeSection();
    size_t getDiagnosticModeSection();
    void setPostSyncOffset();

    enum RingBufferUse : uint32_t {
        FirstBuffer,
        SecondBuffer,
        MaxBuffers
    };

    LinearStream ringCommandStream;
    FlushStamp completionRingBuffers[RingBufferUse::MaxBuffers] = {0ull, 0ull};
    std::unique_ptr<DirectSubmissionDiagnosticsCollector> diagnostic;

    uint64_t semaphoreGpuVa = 0u;
    uint64_t gpuVaForMiFlush = 0u;

    Device &device;
    OsContext &osContext;
    const HardwareInfo *hwInfo = nullptr;
    GraphicsAllocation *ringBuffer = nullptr;
    GraphicsAllocation *ringBuffer2 = nullptr;
    GraphicsAllocation *semaphores = nullptr;
    GraphicsAllocation *workPartitionAllocation = nullptr;
    void *semaphorePtr = nullptr;
    volatile RingSemaphoreData *semaphoreData = nullptr;
    volatile void *workloadModeOneStoreAddress = nullptr;

    uint32_t currentQueueWorkCount = 1u;
    RingBufferUse currentRingBuffer = RingBufferUse::FirstBuffer;
    uint32_t workloadMode = 0;
    uint32_t workloadModeOneExpectedValue = 0u;
    uint32_t activeTiles = 1u;
    uint32_t postSyncOffset = 0u;
    volatile uint32_t reserved = 0u;

    bool ringStart = false;
    bool disableCpuCacheFlush = true;
    bool disableCacheFlush = false;
    bool disableMonitorFence = false;
    bool partitionedMode = false;
    bool partitionConfigSet = true;
    bool useNotifyForPostSync = false;
};
} // namespace NEO
