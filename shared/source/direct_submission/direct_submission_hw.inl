/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "create_direct_submission_hw.inl"

#include <cstring>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DirectSubmissionHw<GfxFamily, Dispatcher>::DirectSubmissionHw(Device &device,
                                                              OsContext &osContext)
    : device(device), osContext(osContext) {

    disableCacheFlush = UllsDefaults::defaultDisableCacheFlush;
    disableMonitorFence = UllsDefaults::defaultDisableMonitorFence;

    if (DebugManager.flags.DirectSubmissionDisableCacheFlush.get() != -1) {
        disableCacheFlush = !!DebugManager.flags.DirectSubmissionDisableCacheFlush.get();
    }

    int32_t disableCacheFlushKey = DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.get();
    if (disableCacheFlushKey != -1) {
        disableCpuCacheFlush = disableCacheFlushKey == 1 ? true : false;
    }

    UNRECOVERABLE_IF(!CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureClflush) && !disableCpuCacheFlush);

    hwInfo = &device.getHardwareInfo();
    createDiagnostic();
    setPostSyncOffset();
}

template <typename GfxFamily, typename Dispatcher>
DirectSubmissionHw<GfxFamily, Dispatcher>::~DirectSubmissionHw() = default;

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::allocateResources() {
    DirectSubmissionAllocations allocations;

    bool isMultiOsContextCapable = osContext.getNumSupportedDevices() > 1u;
    MemoryManager *memoryManager = device.getExecutionEnvironment()->memoryManager.get();
    constexpr size_t minimumRequiredSize = 256 * MemoryConstants::kiloByte;
    constexpr size_t additionalAllocationSize = MemoryConstants::pageSize;
    const auto allocationSize = alignUp(minimumRequiredSize + additionalAllocationSize, MemoryConstants::pageSize64k);
    const AllocationProperties commandStreamAllocationProperties{device.getRootDeviceIndex(),
                                                                 true, allocationSize,
                                                                 AllocationType::RING_BUFFER,
                                                                 isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
    ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    UNRECOVERABLE_IF(ringBuffer == nullptr);
    allocations.push_back(ringBuffer);

    ringBuffer2 = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
    UNRECOVERABLE_IF(ringBuffer2 == nullptr);
    allocations.push_back(ringBuffer2);

    const AllocationProperties semaphoreAllocationProperties{device.getRootDeviceIndex(),
                                                             true, MemoryConstants::pageSize,
                                                             AllocationType::SEMAPHORE_BUFFER,
                                                             isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
    semaphores = memoryManager->allocateGraphicsMemoryWithProperties(semaphoreAllocationProperties);
    UNRECOVERABLE_IF(semaphores == nullptr);
    allocations.push_back(semaphores);

    if (this->workPartitionAllocation != nullptr) {
        allocations.push_back(workPartitionAllocation);
    }

    if (DebugManager.flags.DirectSubmissionPrintBuffers.get()) {
        printf("Ring buffer 1 - gpu address: %" PRIx64 " - %" PRIx64 ", cpu address: %p - %p, size: %zu \n",
               ringBuffer->getGpuAddress(),
               ptrOffset(ringBuffer->getGpuAddress(), ringBuffer->getUnderlyingBufferSize()),
               ringBuffer->getUnderlyingBuffer(),
               ptrOffset(ringBuffer->getUnderlyingBuffer(), ringBuffer->getUnderlyingBufferSize()),
               ringBuffer->getUnderlyingBufferSize());

        printf("Ring buffer 2 - gpu address: %" PRIx64 " - %" PRIx64 ", cpu address: %p - %p, size: %zu \n",
               ringBuffer2->getGpuAddress(),
               ptrOffset(ringBuffer2->getGpuAddress(), ringBuffer2->getUnderlyingBufferSize()),
               ringBuffer2->getUnderlyingBuffer(),
               ptrOffset(ringBuffer2->getUnderlyingBuffer(), ringBuffer2->getUnderlyingBufferSize()),
               ringBuffer2->getUnderlyingBufferSize());
    }

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
    workloadModeOneStoreAddress = static_cast<volatile void *>(&semaphoreData->DiagnosticModeCounter);
    *static_cast<volatile uint32_t *>(workloadModeOneStoreAddress) = 0u;

    this->gpuVaForMiFlush = this->semaphoreGpuVa + offsetof(RingSemaphoreData, miFlushSpace);

    auto ret = makeResourcesResident(allocations);

    return ret && allocateOsResources();
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::makeResourcesResident(DirectSubmissionAllocations &allocations) {
    auto memoryInterface = this->device.getRootDeviceEnvironment().memoryOperationsInterface.get();

    auto ret = memoryInterface->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(allocations), false) == MemoryOperationsStatus::SUCCESS;

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::cpuCachelineFlush(void *ptr, size_t size) {
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

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::initialize(bool submitOnInit, bool useNotify) {
    useNotifyForPostSync = useNotify;
    bool ret = allocateResources();

    initDiagnostic(submitOnInit);
    if (ret && submitOnInit) {
        size_t startBufferSize = Dispatcher::getSizePreemption() +
                                 getSizeSemaphoreSection();

        Dispatcher::dispatchPreemption(ringCommandStream);
        if (this->partitionedMode) {
            startBufferSize += getSizePartitionRegisterConfigurationSection();
            dispatchPartitionRegisterConfiguration();

            this->partitionConfigSet = true;
        }
        if (workloadMode == 1) {
            dispatchDiagnosticModeSection();
            startBufferSize += getDiagnosticModeSection();
        }
        dispatchSemaphoreSection(currentQueueWorkCount);

        ringStart = submit(ringCommandStream.getGraphicsAllocation()->getGpuAddress(), startBufferSize);
        performDiagnosticMode();
        return ringStart;
    }
    return ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::startRingBuffer() {
    if (ringStart) {
        return true;
    }

    size_t startSize = getSizeSemaphoreSection();
    if (!this->partitionConfigSet) {
        startSize += getSizePartitionRegisterConfigurationSection();
    }
    size_t requiredSize = startSize + getSizeDispatch() + getSizeEnd();
    if (ringCommandStream.getAvailableSpace() < requiredSize) {
        switchRingBuffers();
    }
    uint64_t gpuStartVa = getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

    if (!this->partitionConfigSet) {
        dispatchPartitionRegisterConfiguration();
        this->partitionConfigSet = true;
    }

    currentQueueWorkCount++;
    dispatchSemaphoreSection(currentQueueWorkCount);

    ringStart = submit(gpuStartVa, startSize);

    return ringStart;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::stopRingBuffer() {
    if (!ringStart) {
        return true;
    }

    void *flushPtr = ringCommandStream.getSpace(0);
    Dispatcher::dispatchCacheFlush(ringCommandStream, *hwInfo, gpuVaForMiFlush);
    if (disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo, this->useNotifyForPostSync, this->partitionedMode);
    }
    Dispatcher::dispatchStopCommandBuffer(ringCommandStream);

    auto bytesToPad = Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer();
    EncodeNoop<GfxFamily>::emitNoop(ringCommandStream, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(ringCommandStream);

    cpuCachelineFlush(flushPtr, getSizeEnd());

    semaphoreData->QueueWorkCount = currentQueueWorkCount;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    this->handleStopRingBuffer();
    this->ringStart = false;

    return true;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSemaphoreSection(uint32_t value) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    dispatchDisablePrefetcher(true);
    EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                          semaphoreGpuVa,
                                                          value,
                                                          COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    dispatchPrefetchMitigation();
    dispatchDisablePrefetcher(false);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSemaphoreSection() {
    size_t semaphoreSize = EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait();
    semaphoreSize += getSizePrefetchMitigation();
    semaphoreSize += 2 * getSizeDisablePrefetcher();
    return semaphoreSize;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchStartSection(uint64_t gpuStartAddress) {
    Dispatcher::dispatchStartCommandBuffer(ringCommandStream, gpuStartAddress);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeStartSection() {
    return Dispatcher::getSizeStartCommandBuffer();
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSwitchRingBufferSection(uint64_t nextBufferGpuAddress) {
    if (disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo, this->useNotifyForPostSync, this->partitionedMode);
    }
    Dispatcher::dispatchStartCommandBuffer(ringCommandStream, nextBufferGpuAddress);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSwitchRingBufferSection() {
    size_t size = Dispatcher::getSizeStartCommandBuffer();
    if (disableMonitorFence) {
        size += Dispatcher::getSizeMonitorFence(*hwInfo);
    }
    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeEnd() {
    size_t size = Dispatcher::getSizeStopCommandBuffer() +
                  Dispatcher::getSizeCacheFlush(*hwInfo) +
                  (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                  MemoryConstants::cacheLineSize;
    if (disableMonitorFence) {
        size += Dispatcher::getSizeMonitorFence(*hwInfo);
    }
    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline uint64_t DirectSubmissionHw<GfxFamily, Dispatcher>::getCommandBufferPositionGpuAddress(void *position) {
    void *currentBase = ringCommandStream.getCpuBase();

    size_t offset = ptrDiff(position, currentBase);
    return ringCommandStream.getGraphicsAllocation()->getGpuAddress() + static_cast<uint64_t>(offset);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDispatch() {
    size_t size = getSizeSemaphoreSection();
    if (workloadMode == 0) {
        size += getSizeStartSection();
    } else if (workloadMode == 1) {
        size += getDiagnosticModeSection();
    }
    //mode 2 does not dispatch any commands

    if (!disableCacheFlush) {
        size += Dispatcher::getSizeCacheFlush(*hwInfo);
    }
    if (!disableMonitorFence) {
        size += Dispatcher::getSizeMonitorFence(*hwInfo);
    }

    size += getSizeNewResourceHandler();

    return size;
}

template <typename GfxFamily, typename Dispatcher>
void *DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchWorkloadSection(BatchBuffer &batchBuffer) {
    void *currentPosition = ringCommandStream.getSpace(0);

    if (DebugManager.flags.DirectSubmissionPrintBuffers.get()) {
        printf("Client buffer:\n");
        printf("Command buffer allocation - gpu address: %" PRIx64 " - %" PRIx64 ", cpu address: %p - %p, size: %zu \n",
               batchBuffer.commandBufferAllocation->getGpuAddress(),
               ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.commandBufferAllocation->getUnderlyingBufferSize()),
               batchBuffer.commandBufferAllocation->getUnderlyingBuffer(),
               ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.commandBufferAllocation->getUnderlyingBufferSize()),
               batchBuffer.commandBufferAllocation->getUnderlyingBufferSize());
        printf("Command buffer - start gpu address: %" PRIx64 " - %" PRIx64 ", start cpu address: %p - %p, start offset: %zu, used size: %zu \n",
               ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset),
               ptrOffset(ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset), batchBuffer.usedSize),
               ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset),
               ptrOffset(ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset), batchBuffer.usedSize),
               batchBuffer.startOffset,
               batchBuffer.usedSize);
    }

    if (workloadMode == 0) {
        auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
        void *returnCmd = batchBuffer.endCmdPtr;

        dispatchStartSection(commandStreamAddress);
        void *returnPosition = ringCommandStream.getSpace(0);

        setReturnAddress(returnCmd, getCommandBufferPositionGpuAddress(returnPosition));
    } else if (workloadMode == 1) {
        DirectSubmissionDiagnostics::diagnosticModeOneDispatch(diagnostic.get());
        dispatchDiagnosticModeSection();
    }
    //mode 2 does not dispatch any commands

    if (!disableCacheFlush) {
        Dispatcher::dispatchCacheFlush(ringCommandStream, *hwInfo, gpuVaForMiFlush);
    }

    if (!disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo, this->useNotifyForPostSync, this->partitionedMode);
    }

    dispatchSemaphoreSection(currentQueueWorkCount + 1);
    return currentPosition;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp) {
    //for now workloads requiring cache coherency are not supported
    UNRECOVERABLE_IF(batchBuffer.requiresCoherency);

    this->startRingBuffer();

    size_t dispatchSize = getSizeDispatch();
    size_t cycleSize = getSizeSwitchRingBufferSection();
    size_t requiredMinimalSize = dispatchSize + cycleSize + getSizeEnd();

    bool buffersSwitched = false;
    getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

    if (ringCommandStream.getAvailableSpace() < requiredMinimalSize) {
        switchRingBuffers();
        buffersSwitched = true;
    }

    handleNewResourcesSubmission();

    void *currentPosition = dispatchWorkloadSection(batchBuffer);

    cpuCachelineFlush(currentPosition, dispatchSize);
    handleResidency();

    //unblock GPU
    semaphoreData->QueueWorkCount = currentQueueWorkCount;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);
    currentQueueWorkCount++;
    DirectSubmissionDiagnostics::diagnosticModeOneSubmit(diagnostic.get());

    uint64_t flushValue = updateTagValue();
    flushStamp.setStamp(flushValue);

    return ringStart;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::setReturnAddress(void *returnCmd, uint64_t returnAddress) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd = GfxFamily::cmdInitBatchBufferStart;
    cmd.setBatchBufferStartAddress(returnAddress);
    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);

    MI_BATCH_BUFFER_START *returnBBStart = static_cast<MI_BATCH_BUFFER_START *>(returnCmd);
    *returnBBStart = cmd;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::handleNewResourcesSubmission() {
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeNewResourceHandler() {
    return 0u;
}

template <typename GfxFamily, typename Dispatcher>
inline uint64_t DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffers() {
    GraphicsAllocation *nextRingBuffer = switchRingBuffersAllocations();
    void *flushPtr = ringCommandStream.getSpace(0);
    uint64_t currentBufferGpuVa = getCommandBufferPositionGpuAddress(flushPtr);

    if (ringStart) {
        dispatchSwitchRingBufferSection(nextRingBuffer->getGpuAddress());
        cpuCachelineFlush(flushPtr, getSizeSwitchRingBufferSection());
    }

    ringCommandStream.replaceBuffer(nextRingBuffer->getUnderlyingBuffer(), ringCommandStream.getMaxAvailableSpace());
    ringCommandStream.replaceGraphicsAllocation(nextRingBuffer);

    handleSwitchRingBuffers();

    return currentBufferGpuVa;
}

template <typename GfxFamily, typename Dispatcher>
inline GraphicsAllocation *DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffersAllocations() {
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

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::deallocateResources() {
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

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::createDiagnostic() {
    if (directSubmissionDiagnosticAvailable) {
        workloadMode = DebugManager.flags.DirectSubmissionEnableDebugBuffer.get();
        if (workloadMode > 0) {
            disableCacheFlush = DebugManager.flags.DirectSubmissionDisableCacheFlush.get();
            disableMonitorFence = DebugManager.flags.DirectSubmissionDisableMonitorFence.get();
            uint32_t executions = static_cast<uint32_t>(DebugManager.flags.DirectSubmissionDiagnosticExecutionCount.get());
            diagnostic = std::make_unique<DirectSubmissionDiagnosticsCollector>(
                executions,
                workloadMode == 1,
                DebugManager.flags.DirectSubmissionBufferPlacement.get(),
                DebugManager.flags.DirectSubmissionSemaphorePlacement.get(),
                workloadMode,
                disableCacheFlush,
                disableMonitorFence);
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::initDiagnostic(bool &submitOnInit) {
    if (directSubmissionDiagnosticAvailable) {
        if (diagnostic.get()) {
            submitOnInit = true;
            diagnostic->diagnosticModeAllocation();
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::performDiagnosticMode() {
    if (directSubmissionDiagnosticAvailable) {
        if (diagnostic.get()) {
            diagnostic->diagnosticModeDiagnostic();
            if (workloadMode == 1) {
                diagnostic->diagnosticModeOneWait(workloadModeOneStoreAddress, workloadModeOneExpectedValue);
            }
            BatchBuffer dummyBuffer = {};
            FlushStampTracker dummyTracker(true);
            for (uint32_t execution = 0; execution < diagnostic->getExecutionsCount(); execution++) {
                dispatchCommandBuffer(dummyBuffer, dummyTracker);
                if (workloadMode == 1) {
                    diagnostic->diagnosticModeOneWaitCollect(execution, workloadModeOneStoreAddress, workloadModeOneExpectedValue);
                }
            }
            workloadMode = 0;
            disableCacheFlush = UllsDefaults::defaultDisableCacheFlush;
            disableMonitorFence = UllsDefaults::defaultDisableMonitorFence;
            diagnostic.reset(nullptr);
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchDiagnosticModeSection() {
    workloadModeOneExpectedValue++;
    uint64_t storeAddress = semaphoreGpuVa;
    storeAddress += ptrDiff(workloadModeOneStoreAddress, semaphorePtr);
    Dispatcher::dispatchStoreDwordCommand(ringCommandStream, storeAddress, workloadModeOneExpectedValue);
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getDiagnosticModeSection() {
    return Dispatcher::getSizeStoreDwordCommand();
}

} // namespace NEO
