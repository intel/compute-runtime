/*
 * Copyright (C) 2020 Intel Corporation
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
    uint8_t ReservedCacheline[60];
    uint32_t tagAllocation;
    uint8_t ReservedCacheline2[60];
    uint32_t DiagnosticModeCounter;
    uint32_t Reserved0Uint32;
    uint64_t Reserved0Uint64;
    uint8_t ReservedCacheline3[48];
};
static_assert((64u * 3) == sizeof(RingSemaphoreData), "Invalid size for RingSemaphoreData");
#pragma pack()

using DirectSubmissionAllocations = StackVec<GraphicsAllocation *, 8>;

struct TagData {
    uint64_t tagAddress = 0ull;
    uint64_t tagValue = 0ull;
};

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

    bool initialize(bool submitOnInit);

    bool stopRingBuffer();

    bool startRingBuffer();

    bool dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp);

    static std::unique_ptr<DirectSubmissionHw<GfxFamily, Dispatcher>> create(Device &device, OsContext &osContext);

  protected:
    static constexpr size_t prefetchSize = 8 * MemoryConstants::cacheLineSize;
    static constexpr size_t prefetchNoops = prefetchSize / sizeof(uint32_t);
    bool allocateResources();
    void deallocateResources();
    MOCKABLE_VIRTUAL bool makeResourcesResident(DirectSubmissionAllocations &allocations);
    virtual bool allocateOsResources() = 0;
    virtual bool submit(uint64_t gpuAddress, size_t size) = 0;
    virtual bool handleResidency() = 0;
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

    void createDiagnostic();
    void initDiagnostic(bool &submitOnInit);
    MOCKABLE_VIRTUAL void performDiagnosticMode();
    void dispatchDiagnosticModeSection();
    size_t getDiagnosticModeSection();

    enum RingBufferUse : uint32_t {
        FirstBuffer,
        SecondBuffer,
        MaxBuffers
    };

    LinearStream ringCommandStream;
    FlushStamp completionRingBuffers[RingBufferUse::MaxBuffers] = {0ull, 0ull};
    std::unique_ptr<DirectSubmissionDiagnosticsCollector> diagnostic;

    uint64_t semaphoreGpuVa = 0u;

    Device &device;
    OsContext &osContext;
    const HardwareInfo *hwInfo = nullptr;
    GraphicsAllocation *ringBuffer = nullptr;
    GraphicsAllocation *ringBuffer2 = nullptr;
    GraphicsAllocation *semaphores = nullptr;
    void *semaphorePtr = nullptr;
    volatile RingSemaphoreData *semaphoreData = nullptr;
    volatile void *workloadModeOneStoreAddress = nullptr;

    uint32_t currentQueueWorkCount = 1u;
    RingBufferUse currentRingBuffer = RingBufferUse::FirstBuffer;
    uint32_t workloadMode = 0;
    uint32_t workloadModeOneExpectedValue = 0u;

    static constexpr bool defaultDisableCacheFlush = false;
    static constexpr bool defaultDisableMonitorFence = false;

    bool ringStart = false;
    bool disableCpuCacheFlush = false;
    bool disableCacheFlush = false;
    bool disableMonitorFence = false;
};
} // namespace NEO
