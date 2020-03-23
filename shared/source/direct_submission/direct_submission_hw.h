/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>

namespace NEO {

#pragma pack(1)
struct RingSemaphoreData {
    uint32_t QueueWorkCount;
    uint8_t ReservedCacheline[60];
    uint32_t Reserved1Uint32;
    uint32_t Reserved2Uint32;
    uint32_t Reserved3Uint32;
    uint32_t Reserved4Uint32;
    uint64_t Reserved1Uint64;
    uint64_t Reserved2Uint64;
};
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
class Device;
class Dispatcher;
class OsContext;

template <typename GfxFamily>
class DirectSubmissionHw {
  public:
    DirectSubmissionHw(Device &device,
                       std::unique_ptr<Dispatcher> cmdDispatcher,
                       OsContext &osContext);

    virtual ~DirectSubmissionHw();

    bool initialize(bool submitOnInit);

    bool stopRingBuffer();

    bool startRingBuffer();

    bool dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp);

  protected:
    static constexpr size_t prefetchSize = 8 * MemoryConstants::cacheLineSize;
    bool allocateResources();
    void deallocateResources();
    virtual bool allocateOsResources(DirectSubmissionAllocations &allocations) = 0;
    virtual bool submit(uint64_t gpuAddress, size_t size) = 0;
    virtual bool handleResidency() = 0;
    virtual uint64_t switchRingBuffers() = 0;
    GraphicsAllocation *switchRingBuffersAllocations();
    virtual uint64_t updateTagValue() = 0;
    virtual void getTagAddressValue(TagData &tagData) = 0;

    void cpuCachelineFlush(void *ptr, size_t size);

    void *dispatchSemaphoreSection(uint32_t value);
    size_t getSizeSemaphoreSection();

    void dispatchStartSection(uint64_t gpuStartAddress);
    size_t getSizeStartSection();

    void dispatchSwitchRingBufferSection(uint64_t nextBufferGpuAddress);
    size_t getSizeSwitchRingBufferSection();

    void *dispatchFlushSection();
    size_t getSizeFlushSection();

    void *dispatchTagUpdateSection(uint64_t address, uint64_t value);
    size_t getSizeTagUpdateSection();

    void dispatchEndingSection();
    size_t getSizeEndingSection();

    void setReturnAddress(void *returnCmd, uint64_t returnAddress);

    void *dispatchWorkloadSection(BatchBuffer &batchBuffer);
    size_t getSizeDispatch();

    void dispatchStoreDataSection(uint64_t gpuVa, uint32_t value);
    size_t getSizeStoraDataSection();

    size_t getSizeEnd();

    uint64_t getCommandBufferPositionGpuAddress(void *position);

    enum RingBufferUse : uint32_t {
        FirstBuffer,
        SecondBuffer,
        MaxBuffers
    };

    LinearStream ringCommandStream;
    std::unique_ptr<Dispatcher> cmdDispatcher;
    FlushStamp completionRingBuffers[RingBufferUse::MaxBuffers] = {0ull, 0ull};

    uint64_t semaphoreGpuVa = 0u;

    Device &device;
    OsContext &osContext;
    const HardwareInfo *hwInfo = nullptr;
    GraphicsAllocation *ringBuffer = nullptr;
    GraphicsAllocation *ringBuffer2 = nullptr;
    GraphicsAllocation *semaphores = nullptr;
    void *semaphorePtr = nullptr;
    volatile RingSemaphoreData *semaphoreData = nullptr;

    uint32_t currentQueueWorkCount = 1u;
    RingBufferUse currentRingBuffer = RingBufferUse::FirstBuffer;
    uint32_t workloadMode = 0;

    bool ringStart = false;
    bool disableCpuCacheFlush = false;
    bool disableCacheFlush = false;
    bool disableMonitorFence = false;
};
} // namespace NEO
