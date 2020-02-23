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

    size_t getSizeDispatch();

    size_t getSizeEnd();

    uint64_t getCommandBufferPositionGpuAddress(void *position);

    Device &device;
    OsContext &osContext;
    std::unique_ptr<Dispatcher> cmdDispatcher;
    const HardwareInfo *hwInfo = nullptr;

    enum RingBufferUse : uint32_t {
        FirstBuffer,
        SecondBuffer,
        MaxBuffers
    };
    GraphicsAllocation *ringBuffer = nullptr;
    GraphicsAllocation *ringBuffer2 = nullptr;
    FlushStamp completionRingBuffers[RingBufferUse::MaxBuffers] = {0ull, 0ull};
    RingBufferUse currentRingBuffer = RingBufferUse::FirstBuffer;
    LinearStream ringCommandStream;

    GraphicsAllocation *semaphores = nullptr;
    void *semaphorePtr = nullptr;
    uint64_t semaphoreGpuVa = 0u;
    volatile RingSemaphoreData *semaphoreData = nullptr;
    uint32_t currentQueueWorkCount = 1u;

    bool ringStart = false;

    bool disableCpuCacheFlush = false;
};
} // namespace NEO
