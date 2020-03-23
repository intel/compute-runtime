/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/dispatchers/dispatcher.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include <cstring>

namespace NEO {

template <typename GfxFamily>
DirectSubmissionHw<GfxFamily>::DirectSubmissionHw(Device &device,
                                                  std::unique_ptr<Dispatcher> cmdDispatcher,
                                                  OsContext &osContext)
    : device(device), osContext(osContext) {
    UNRECOVERABLE_IF(!CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureClflush));

    this->cmdDispatcher = std::move(cmdDispatcher);
    int32_t disableCacheFlushKey = DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.get();
    if (disableCacheFlushKey != -1) {
        disableCpuCacheFlush = disableCacheFlushKey == 1 ? true : false;
    }
    workloadMode = DebugManager.flags.DirectSubmissionEnableDebugBuffer.get();
    disableCacheFlush = DebugManager.flags.DirectSubmissionDisableCacheFlush.get();
    disableMonitorFence = DebugManager.flags.DirectSubmissionDisableMonitorFence.get();
    hwInfo = &device.getHardwareInfo();
}

template <typename GfxFamily>
DirectSubmissionHw<GfxFamily>::~DirectSubmissionHw() {
}

template <typename GfxFamily>
bool DirectSubmissionHw<GfxFamily>::allocateResources() {
    DirectSubmissionAllocations allocations;

    bool isMultiOsContextCapable = osContext.getNumSupportedDevices() > 1u;
    MemoryManager *memoryManager = device.getExecutionEnvironment()->memoryManager.get();
    constexpr size_t minimumRequiredSize = 256 * MemoryConstants::kiloByte;
    constexpr size_t additionalAllocationSize = MemoryConstants::pageSize;
    const auto allocationSize = alignUp(minimumRequiredSize + additionalAllocationSize, MemoryConstants::pageSize64k);
    const AllocationProperties commandStreamAllocationProperties{device.getRootDeviceIndex(),
                                                                 true, allocationSize,
                                                                 GraphicsAllocation::AllocationType::RING_BUFFER,
                                                                 isMultiOsContextCapable};
    ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    UNRECOVERABLE_IF(ringBuffer == nullptr);
    allocations.push_back(ringBuffer);

    ringBuffer2 = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    UNRECOVERABLE_IF(ringBuffer2 == nullptr);
    allocations.push_back(ringBuffer2);

    const AllocationProperties semaphoreAllocationProperties{device.getRootDeviceIndex(),
                                                             true, MemoryConstants::pageSize,
                                                             GraphicsAllocation::AllocationType::SEMAPHORE_BUFFER,
                                                             isMultiOsContextCapable};
    semaphores = memoryManager->allocateGraphicsMemoryWithProperties(semaphoreAllocationProperties);
    UNRECOVERABLE_IF(semaphores == nullptr);
    allocations.push_back(semaphores);

    handleResidency();
    ringCommandStream.replaceBuffer(ringBuffer->getUnderlyingBuffer(), minimumRequiredSize);
    ringCommandStream.replaceGraphicsAllocation(ringBuffer);

    memset(ringBuffer->getUnderlyingBuffer(), 0, allocationSize);
    memset(ringBuffer2->getUnderlyingBuffer(), 0, allocationSize);
    semaphorePtr = semaphores->getUnderlyingBuffer();
    semaphoreGpuVa = semaphores->getGpuAddress();
    semaphoreData = static_cast<volatile RingSemaphoreData *>(semaphorePtr);
    memset(semaphorePtr, 0, sizeof(RingSemaphoreData));
    semaphoreData->QueueWorkCount = 0;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    return allocateOsResources(allocations);
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::cpuCachelineFlush(void *ptr, size_t size) {
    if (disableCpuCacheFlush) {
        return;
    }
    constexpr size_t cachlineBit = 6;
    static_assert(MemoryConstants::cacheLineSize == 1 << cachlineBit, "cachlineBit has invalid value");
    char *flushPtr = reinterpret_cast<char *>(ptr);
    char *flushEndPtr = reinterpret_cast<char *>(ptr) + size;

    flushPtr = alignDown(flushPtr, MemoryConstants::cacheLineSize);
    flushEndPtr = alignUp(flushEndPtr, MemoryConstants::cacheLineSize);
    size_t cachelines = (flushEndPtr - flushPtr) >> cachlineBit;
    for (size_t i = 0; i < cachelines; i++) {
        CpuIntrinsics::clFlush(flushPtr);
        flushPtr += MemoryConstants::cacheLineSize;
    }
}

template <typename GfxFamily>
bool DirectSubmissionHw<GfxFamily>::initialize(bool submitOnInit) {
    bool ret = allocateResources();
    if (ret && submitOnInit) {
        size_t startBufferSize = cmdDispatcher->getSizePreemption() +
                                 getSizeSemaphoreSection();
        cmdDispatcher->dispatchPreemption(ringCommandStream);
        dispatchSemaphoreSection(currentQueueWorkCount);

        ringStart = submit(ringCommandStream.getGraphicsAllocation()->getGpuAddress(), startBufferSize);
        return ringStart;
    }
    return ret;
}

template <typename GfxFamily>
bool DirectSubmissionHw<GfxFamily>::startRingBuffer() {
    if (ringStart) {
        return true;
    }
    size_t startSize = getSizeSemaphoreSection();
    size_t requiredSize = startSize + getSizeDispatch() + getSizeEnd();
    if (ringCommandStream.getAvailableSpace() < requiredSize) {
        switchRingBuffers();
    }
    uint64_t gpuStartVa = getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

    currentQueueWorkCount++;
    dispatchSemaphoreSection(currentQueueWorkCount);

    ringStart = submit(gpuStartVa, startSize);

    return ringStart;
}

template <typename GfxFamily>
bool DirectSubmissionHw<GfxFamily>::stopRingBuffer() {
    void *flushPtr = ringCommandStream.getSpace(0);
    dispatchFlushSection();
    if (disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        dispatchTagUpdateSection(currentTagData.tagAddress, currentTagData.tagValue);
    }
    dispatchEndingSection();
    cpuCachelineFlush(flushPtr, getSizeEnd());

    semaphoreData->QueueWorkCount = currentQueueWorkCount;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    return true;
}

template <typename GfxFamily>
inline void *DirectSubmissionHw<GfxFamily>::dispatchSemaphoreSection(uint32_t value) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    void *semaphorePosition = ringCommandStream.getSpace(0);

    EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                          semaphoreGpuVa,
                                                          value,
                                                          COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    void *prefetchSpace = ringCommandStream.getSpace(prefetchSize);
    memset(prefetchSpace, 0, prefetchSize);
    return semaphorePosition;
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeSemaphoreSection() {
    size_t semaphoreSize = EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait();
    return (semaphoreSize + prefetchSize);
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::dispatchStartSection(uint64_t gpuStartAddress) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    auto bbufferStart = ringCommandStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *bbufferStart = GfxFamily::cmdInitBatchBufferStart;

    bbufferStart->setBatchBufferStartAddressGraphicsaddress472(gpuStartAddress);
    bbufferStart->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeStartSection() {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    size_t size = sizeof(MI_BATCH_BUFFER_START);
    return size;
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::dispatchSwitchRingBufferSection(uint64_t nextBufferGpuAddress) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    auto bbufferStart = ringCommandStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    *bbufferStart = GfxFamily::cmdInitBatchBufferStart;

    bbufferStart->setBatchBufferStartAddressGraphicsaddress472(nextBufferGpuAddress);
    bbufferStart->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeSwitchRingBufferSection() {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    return sizeof(MI_BATCH_BUFFER_START);
}

template <typename GfxFamily>
inline void *DirectSubmissionHw<GfxFamily>::dispatchFlushSection() {
    void *currentPosition = ringCommandStream.getSpace(0);
    cmdDispatcher->dispatchCacheFlush(ringCommandStream, *hwInfo);
    return currentPosition;
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeFlushSection() {
    return cmdDispatcher->getSizeCacheFlush(*hwInfo);
}

template <typename GfxFamily>
inline void *DirectSubmissionHw<GfxFamily>::dispatchTagUpdateSection(uint64_t address, uint64_t value) {
    void *currentPosition = ringCommandStream.getSpace(0);
    cmdDispatcher->dispatchMonitorFence(ringCommandStream, address, value, *hwInfo);
    return currentPosition;
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeTagUpdateSection() {
    size_t size = cmdDispatcher->getSizeMonitorFence(*hwInfo);
    return size;
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::dispatchEndingSection() {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto bbufferEnd = ringCommandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *bbufferEnd = GfxFamily::cmdInitBatchBufferEnd;
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeEndingSection() {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    return sizeof(MI_BATCH_BUFFER_END);
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::dispatchStoreDataSection(uint64_t gpuVa, uint32_t value) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    auto storeDataImmediate = ringCommandStream.getSpaceForCmd<MI_STORE_DATA_IMM>();
    *storeDataImmediate = GfxFamily::cmdInitStoreDataImm;

    storeDataImmediate->setAddress(gpuVa);
    storeDataImmediate->setDataDword0(value);
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeStoraDataSection() {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    return sizeof(MI_STORE_DATA_IMM);
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeEnd() {
    size_t size = getSizeEndingSection() +
                  getSizeFlushSection();
    if (disableMonitorFence) {
        size += getSizeTagUpdateSection();
    }
    return size;
}

template <typename GfxFamily>
inline uint64_t DirectSubmissionHw<GfxFamily>::getCommandBufferPositionGpuAddress(void *position) {
    void *currentBase = ringCommandStream.getCpuBase();

    size_t offset = ptrDiff(position, currentBase);
    return ringCommandStream.getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(offset);
}

template <typename GfxFamily>
inline size_t DirectSubmissionHw<GfxFamily>::getSizeDispatch() {
    size_t size = getSizeSemaphoreSection();
    if (workloadMode == 0) {
        size += getSizeStartSection();
    } else if (workloadMode == 1) {
        size += getSizeStoraDataSection();
    }
    //mode 2 does not dispatch any commands

    if (!disableCacheFlush) {
        size += getSizeFlushSection();
    }
    if (!disableMonitorFence) {
        size += getSizeTagUpdateSection();
    }

    return size;
}

template <typename GfxFamily>
void *DirectSubmissionHw<GfxFamily>::dispatchWorkloadSection(BatchBuffer &batchBuffer) {
    void *currentPosition = ringCommandStream.getSpace(0);

    if (workloadMode == 0) {
        auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
        void *returnCmd = batchBuffer.endCmdPtr;

        dispatchStartSection(commandStreamAddress);
        void *returnPosition = ringCommandStream.getSpace(0);

        setReturnAddress(returnCmd, getCommandBufferPositionGpuAddress(returnPosition));
    } else if (workloadMode == 1) {
        uint64_t storeAddress = semaphoreGpuVa;
        storeAddress += ptrDiff(&static_cast<RingSemaphoreData *>(semaphorePtr)->Reserved1Uint32, semaphorePtr);
        dispatchStoreDataSection(storeAddress, currentQueueWorkCount);
    }
    //mode 2 does not dispatch any commands

    if (!disableCacheFlush) {
        dispatchFlushSection();
    }

    if (!disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        dispatchTagUpdateSection(currentTagData.tagAddress, currentTagData.tagValue);
    }

    dispatchSemaphoreSection(currentQueueWorkCount + 1);
    return currentPosition;
}

template <typename GfxFamily>
bool DirectSubmissionHw<GfxFamily>::dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp) {
    size_t dispatchSize = getSizeDispatch();
    size_t cycleSize = getSizeSwitchRingBufferSection();
    size_t requiredMinimalSize = dispatchSize + cycleSize + getSizeEnd();

    bool buffersSwitched = false;
    uint64_t startGpuVa = getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

    if (ringCommandStream.getAvailableSpace() < requiredMinimalSize) {
        startGpuVa = switchRingBuffers();
        buffersSwitched = true;
    }

    void *currentPosition = dispatchWorkloadSection(batchBuffer);

    if (ringStart) {
        cpuCachelineFlush(currentPosition, dispatchSize);
        handleResidency();
    }

    //unblock GPU
    semaphoreData->QueueWorkCount = currentQueueWorkCount;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);
    currentQueueWorkCount++;

    //whem ring buffer is not started at init or not restarted periodically
    if (!ringStart) {
        size_t submitSize = dispatchSize;
        if (buffersSwitched) {
            submitSize = cycleSize;
        }
        ringStart = submit(startGpuVa, submitSize);
    }

    uint64_t flushValue = updateTagValue();
    flushStamp.setStamp(flushValue);

    return ringStart;
}

template <typename GfxFamily>
inline void DirectSubmissionHw<GfxFamily>::setReturnAddress(void *returnCmd, uint64_t returnAddress) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START *returnBBStart = static_cast<MI_BATCH_BUFFER_START *>(returnCmd);
    returnBBStart->setBatchBufferStartAddressGraphicsaddress472(returnAddress);
}

template <typename GfxFamily>
inline GraphicsAllocation *DirectSubmissionHw<GfxFamily>::switchRingBuffersAllocations() {
    GraphicsAllocation *nextAllocation = nullptr;
    if (currentRingBuffer == RingBufferUse::FirstBuffer) {
        nextAllocation = ringBuffer2;
        currentRingBuffer = RingBufferUse::SecondBuffer;
    } else {
        nextAllocation = ringBuffer;
        currentRingBuffer = RingBufferUse::FirstBuffer;
    }
    return nextAllocation;
}

template <typename GfxFamily>
void DirectSubmissionHw<GfxFamily>::deallocateResources() {
    MemoryManager *memoryManager = device.getExecutionEnvironment()->memoryManager.get();

    if (ringBuffer) {
        memoryManager->freeGraphicsMemory(ringBuffer);
        ringBuffer = nullptr;
    }
    if (ringBuffer2) {
        memoryManager->freeGraphicsMemory(ringBuffer2);
        ringBuffer2 = nullptr;
    }
    if (semaphores) {
        memoryManager->freeGraphicsMemory(semaphores);
        semaphores = nullptr;
    }
}

} // namespace NEO
