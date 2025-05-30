/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/cpu_info.h"

#include "create_direct_submission_hw.inl"

#include <algorithm>
#include <cstring>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DirectSubmissionHw<GfxFamily, Dispatcher>::DirectSubmissionHw(const DirectSubmissionInputParams &inputParams)
    : ringBuffers(RingBufferUse::initialRingBufferCount), osContext(inputParams.osContext), rootDeviceIndex(inputParams.rootDeviceIndex), rootDeviceEnvironment(inputParams.rootDeviceEnvironment) {
    memoryManager = inputParams.memoryManager;
    globalFenceAllocation = inputParams.globalFenceAllocation;
    hwInfo = inputParams.rootDeviceEnvironment.getHardwareInfo();
    memoryOperationHandler = inputParams.rootDeviceEnvironment.memoryOperationsInterface.get();

    auto &productHelper = inputParams.rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &compilerProductHelper = inputParams.rootDeviceEnvironment.getHelper<CompilerProductHelper>();

    disableCacheFlush = UllsDefaults::defaultDisableCacheFlush;
    disableMonitorFence = UllsDefaults::defaultDisableMonitorFence;
    if (debugManager.flags.DirectSubmissionDisableMonitorFence.get() != -1) {
        this->disableMonitorFence = debugManager.flags.DirectSubmissionDisableMonitorFence.get();
    }

    if (debugManager.flags.DirectSubmissionMaxRingBuffers.get() != -1) {
        this->maxRingBufferCount = debugManager.flags.DirectSubmissionMaxRingBuffers.get();
    }

    if (debugManager.flags.DirectSubmissionDisableCacheFlush.get() != -1) {
        disableCacheFlush = !!debugManager.flags.DirectSubmissionDisableCacheFlush.get();
    }

    if (debugManager.flags.DirectSubmissionDetectGpuHang.get() != -1) {
        detectGpuHang = !!debugManager.flags.DirectSubmissionDetectGpuHang.get();
    }

    miMemFenceRequired = productHelper.isAcquireGlobalFenceInDirectSubmissionRequired(*hwInfo);

    if (debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() != -1) {
        miMemFenceRequired = debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get();
    }

    if (miMemFenceRequired && compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled(*hwInfo))) {
        this->systemMemoryFenceAddressSet = true;
    }

    if (debugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.get() != -1) {
        sfenceMode = static_cast<DirectSubmissionSfenceMode>(debugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.get());
    }

    if (debugManager.flags.DirectSubmissionMonitorFenceInputPolicy.get() != -1) {
        this->inputMonitorFenceDispatchRequirement = !!(debugManager.flags.DirectSubmissionMonitorFenceInputPolicy.get());
    }

    int32_t disableCacheFlushKey = debugManager.flags.DirectSubmissionDisableCpuCacheFlush.get();
    if (disableCacheFlushKey != -1) {
        disableCpuCacheFlush = (disableCacheFlushKey == 1);
    }

    isDisablePrefetcherRequired = productHelper.isPrefetcherDisablingInDirectSubmissionRequired();
    if (debugManager.flags.DirectSubmissionDisablePrefetcher.get() != -1) {
        isDisablePrefetcherRequired = !!debugManager.flags.DirectSubmissionDisablePrefetcher.get();
    }

    UNRECOVERABLE_IF(!CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureClflush) && !disableCpuCacheFlush);

    setImmWritePostSyncOffset();

    dcFlushRequired = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, inputParams.rootDeviceEnvironment);
    auto &gfxCoreHelper = inputParams.rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    relaxedOrderingEnabled = gfxCoreHelper.isRelaxedOrderingSupported();

    this->currentRelaxedOrderingQueueSize = RelaxedOrderingHelper::queueSizeMultiplier;

    if (debugManager.flags.DirectSubmissionRelaxedOrdering.get() != -1) {
        relaxedOrderingEnabled = (debugManager.flags.DirectSubmissionRelaxedOrdering.get() == 1);
    }

    if (Dispatcher::isCopy() && relaxedOrderingEnabled) {
        relaxedOrderingEnabled = (debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.get() != 0);
    }
}

template <typename GfxFamily, typename Dispatcher>
DirectSubmissionHw<GfxFamily, Dispatcher>::~DirectSubmissionHw() = default;

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::allocateResources() {
    DirectSubmissionAllocations allocations;

    bool isMultiOsContextCapable = osContext.getNumSupportedDevices() > 1u;
    constexpr size_t minimumRequiredSize = 256 * MemoryConstants::kiloByte;
    constexpr size_t additionalAllocationSize = MemoryConstants::pageSize;
    const auto allocationSize = alignUp(minimumRequiredSize + additionalAllocationSize, MemoryConstants::pageSize64k);
    const AllocationProperties commandStreamAllocationProperties{rootDeviceIndex,
                                                                 true, allocationSize,
                                                                 AllocationType::ringBuffer,
                                                                 isMultiOsContextCapable, false, osContext.getDeviceBitfield()};

    for (uint32_t ringBufferIndex = 0; ringBufferIndex < RingBufferUse::initialRingBufferCount; ringBufferIndex++) {
        auto ringBuffer = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
        this->ringBuffers[ringBufferIndex].ringBuffer = ringBuffer;
        UNRECOVERABLE_IF(ringBuffer == nullptr);
        allocations.push_back(ringBuffer);
        memset(ringBuffer->getUnderlyingBuffer(), 0, allocationSize);
    }

    const AllocationProperties semaphoreAllocationProperties{rootDeviceIndex,
                                                             true, MemoryConstants::pageSize,
                                                             AllocationType::semaphoreBuffer,
                                                             isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
    semaphores = memoryManager->allocateGraphicsMemoryWithProperties(semaphoreAllocationProperties);
    UNRECOVERABLE_IF(semaphores == nullptr);
    allocations.push_back(semaphores);

    if (this->workPartitionAllocation != nullptr) {
        allocations.push_back(workPartitionAllocation);
    }

    if (completionFenceAllocation != nullptr) {
        allocations.push_back(completionFenceAllocation);
    }

    if (this->relaxedOrderingEnabled) {
        const AllocationProperties allocationProperties(rootDeviceIndex,
                                                        true, MemoryConstants::pageSize64k,
                                                        AllocationType::deferredTasksList,
                                                        isMultiOsContextCapable, false, osContext.getDeviceBitfield());

        deferredTasksListAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
        UNRECOVERABLE_IF(deferredTasksListAllocation == nullptr);

        allocations.push_back(deferredTasksListAllocation);

        AllocationProperties relaxedOrderingSchedulerAllocationProperties(rootDeviceIndex,
                                                                          true, MemoryConstants::pageSize64k,
                                                                          AllocationType::commandBuffer,
                                                                          isMultiOsContextCapable, false, osContext.getDeviceBitfield());
        relaxedOrderingSchedulerAllocationProperties.flags.cantBeReadOnly = true;
        relaxedOrderingSchedulerAllocation = memoryManager->allocateGraphicsMemoryWithProperties(relaxedOrderingSchedulerAllocationProperties);
        UNRECOVERABLE_IF(relaxedOrderingSchedulerAllocation == nullptr);

        allocations.push_back(relaxedOrderingSchedulerAllocation);
    }

    if (debugManager.flags.DirectSubmissionPrintBuffers.get()) {
        for (uint32_t ringBufferIndex = 0; ringBufferIndex < RingBufferUse::initialRingBufferCount; ringBufferIndex++) {
            const auto ringBuffer = this->ringBuffers[ringBufferIndex].ringBuffer;

            printf("Ring buffer %u - gpu address: %" PRIx64 " - %" PRIx64 ", cpu address: %p - %p, size: %zu \n",
                   ringBufferIndex,
                   ringBuffer->getGpuAddress(),
                   ptrOffset(ringBuffer->getGpuAddress(), ringBuffer->getUnderlyingBufferSize()),
                   ringBuffer->getUnderlyingBuffer(),
                   ptrOffset(ringBuffer->getUnderlyingBuffer(), ringBuffer->getUnderlyingBufferSize()),
                   ringBuffer->getUnderlyingBufferSize());
        }
    }

    handleResidency();
    ringCommandStream.replaceBuffer(this->ringBuffers[0u].ringBuffer->getUnderlyingBuffer(), minimumRequiredSize);
    ringCommandStream.replaceGraphicsAllocation(this->ringBuffers[0].ringBuffer);

    semaphorePtr = semaphores->getUnderlyingBuffer();
    semaphoreGpuVa = semaphores->getGpuAddress();
    semaphoreData = static_cast<volatile RingSemaphoreData *>(semaphorePtr);
    memset(semaphorePtr, 0, sizeof(RingSemaphoreData));
    semaphoreData->queueWorkCount = 0;
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    this->gpuVaForMiFlush = this->semaphoreGpuVa + offsetof(RingSemaphoreData, miFlushSpace);
    this->gpuVaForPagingFenceSemaphore = this->semaphoreGpuVa + offsetof(RingSemaphoreData, pagingFenceCounter);
    auto ret = makeResourcesResident(allocations);

    return ret && allocateOsResources();
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::makeResourcesResident(DirectSubmissionAllocations &allocations) {
    auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(allocations), false, false, true) == MemoryOperationsStatus::success;

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::allocateOsResources() {
    if (this->semaphorePtr != nullptr) {
        this->tagAddress = reinterpret_cast<volatile TagAddressType *>(reinterpret_cast<uint8_t *>(this->semaphorePtr) + offsetof(RingSemaphoreData, tagAllocation));
    } else {
        this->tagAddress = nullptr;
    }
    return true;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::unblockGpu() {
    SemaphoreFenceHelper fence(*this);

    if (this->pciBarrierPtr) {
        *this->pciBarrierPtr = 0u;
    }

    PRINT_DEBUG_STRING(debugManager.flags.DirectSubmissionPrintSemaphoreUsage.get() == 1, stdout, "DirectSubmission semaphore %" PRIx64 " unlocked with value: %u\n", semaphoreGpuVa, currentQueueWorkCount);

    semaphoreData->queueWorkCount = currentQueueWorkCount;
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
bool DirectSubmissionHw<GfxFamily, Dispatcher>::initialize(bool submitOnInit) {
    bool ret = allocateResources();

    if (ret && submitOnInit) {
        size_t startBufferSize = Dispatcher::getSizePreemption() +
                                 getSizeSemaphoreSection(false);

        Dispatcher::dispatchPreemption(ringCommandStream);
        if (this->partitionedMode) {
            startBufferSize += getSizePartitionRegisterConfigurationSection();
            dispatchPartitionRegisterConfiguration();

            this->partitionConfigSet = true;
        }
        if (this->globalFenceAllocation && !this->systemMemoryFenceAddressSet) {
            startBufferSize += getSizeSystemMemoryFenceAddress();
            dispatchSystemMemoryFenceAddress();

            this->systemMemoryFenceAddressSet = true;
        }
        if (this->relaxedOrderingEnabled) {
            preinitializeRelaxedOrderingSections();

            initRelaxedOrderingRegisters();
            dispatchStaticRelaxedOrderingScheduler();
            startBufferSize += RelaxedOrderingHelper::getSizeRegistersInit<GfxFamily>();

            this->relaxedOrderingInitialized = true;
        }

        dispatchSemaphoreSection(currentQueueWorkCount);

        ringStart = submit(ringCommandStream.getGraphicsAllocation()->getGpuAddress(), startBufferSize, nullptr);
        return ringStart;
    }
    return ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::stopRingBuffer(bool blocking) {
    if (!ringStart) {
        if (blocking) {
            this->ensureRingCompletion();
        }
        return true;
    }

    bool relaxedOrderingSchedulerWasRequired = this->relaxedOrderingSchedulerRequired;
    if (this->relaxedOrderingEnabled && this->relaxedOrderingSchedulerRequired) {
        dispatchRelaxedOrderingQueueStall();
    }

    void *flushPtr = ringCommandStream.getSpace(0);
    Dispatcher::dispatchCacheFlush(ringCommandStream, this->rootDeviceEnvironment, gpuVaForMiFlush);
    if (disableMonitorFence) {
        dispatchStopRingBufferSection();
    }
    Dispatcher::dispatchStopCommandBuffer(ringCommandStream);

    auto bytesToPad = Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer();
    EncodeNoop<GfxFamily>::emitNoop(ringCommandStream, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(ringCommandStream);

    cpuCachelineFlush(flushPtr, getSizeEnd(relaxedOrderingSchedulerWasRequired));
    this->unblockGpu();
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    this->handleStopRingBuffer();
    this->ringStart = false;

    if (blocking) {
        this->ensureRingCompletion();
    }

    return true;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSemaphoreSection(uint32_t value) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    if (debugManager.flags.DirectSubmissionPrintSemaphoreUsage.get() == 1) {
        printf("DirectSubmission semaphore %" PRIx64 " programmed with value: %u\n", semaphoreGpuVa, value);
    }

    dispatchDisablePrefetcher(true);

    if (this->relaxedOrderingEnabled && this->relaxedOrderingSchedulerRequired) {
        dispatchRelaxedOrderingSchedulerSection(value);
    } else {
        bool switchOnUnsuccessful = false;

        if (debugManager.flags.DirectSubmissionSwitchSemaphoreMode.get() != -1) {
            switchOnUnsuccessful = !!debugManager.flags.DirectSubmissionSwitchSemaphoreMode.get();
        }

        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                              semaphoreGpuVa,
                                                              value,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, switchOnUnsuccessful, nullptr);
    }

    if (miMemFenceRequired) {
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronizationForDirectSubmission(ringCommandStream, this->gpuVaForAdditionalSynchronizationWA, NEO::FenceType::acquire, rootDeviceEnvironment);
    }

    dispatchPrefetchMitigation();
    dispatchDisablePrefetcher(false);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSemaphoreSection(bool relaxedOrderingSchedulerRequired) {
    size_t semaphoreSize = (this->relaxedOrderingEnabled && relaxedOrderingSchedulerRequired) ? RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize
                                                                                              : EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
    semaphoreSize += getSizePrefetchMitigation();

    if (isDisablePrefetcherRequired) {
        semaphoreSize += 2 * getSizeDisablePrefetcher();
    }

    if (miMemFenceRequired) {
        semaphoreSize += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(rootDeviceEnvironment);
    }

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
        getTagAddressValueForRingSwitch(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
    }
    Dispatcher::dispatchStartCommandBuffer(ringCommandStream, nextBufferGpuAddress);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSwitchRingBufferSection() {
    size_t size = Dispatcher::getSizeStartCommandBuffer();
    if (disableMonitorFence) {
        size += Dispatcher::getSizeMonitorFence(rootDeviceEnvironment);
    }
    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeEnd(bool relaxedOrderingSchedulerRequired) {
    size_t size = Dispatcher::getSizeStopCommandBuffer() +
                  Dispatcher::getSizeCacheFlush(rootDeviceEnvironment) +
                  (Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer()) +
                  MemoryConstants::cacheLineSize;
    if (disableMonitorFence) {
        size += dispatchStopRingBufferSectionSize();
    }
    if (this->relaxedOrderingEnabled && relaxedOrderingSchedulerRequired) {
        size += getSizeDispatchRelaxedOrderingQueueStall();
    }
    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDispatch(bool relaxedOrderingSchedulerRequired, bool returnPtrsRequired, bool dispatchMonitorFence) {
    size_t size = getSizeSemaphoreSection(relaxedOrderingSchedulerRequired) + getSizeStartSection();
    if (this->relaxedOrderingEnabled && returnPtrsRequired) {
        size += RelaxedOrderingHelper::getSizeReturnPtrRegs<GfxFamily>();
    }

    if (!disableCacheFlush) {
        size += Dispatcher::getSizeCacheFlush(rootDeviceEnvironment);
    }
    if (dispatchMonitorFence) {
        size += Dispatcher::getSizeMonitorFence(rootDeviceEnvironment);
    }

    size += getSizeNewResourceHandler();

    return size;
}

template <typename GfxFamily, typename Dispatcher>
void *DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchWorkloadSection(BatchBuffer &batchBuffer, bool dispatchMonitorFence) {
    void *currentPosition = ringCommandStream.getSpace(0);
    auto copyCmdBuffer = this->copyCommandBufferIntoRing(batchBuffer);

    if (debugManager.flags.DirectSubmissionPrintBuffers.get()) {
        printf("Client buffer:\n");
        printf("Command buffer allocation - gpu address: %" PRIx64 " - %" PRIx64 ", cpu address: %p - %p, size: %zu \n",
               batchBuffer.commandBufferAllocation->getGpuAddress(),
               ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.commandBufferAllocation->getUnderlyingBufferSize()),
               batchBuffer.commandBufferAllocation->getUnderlyingBuffer(),
               ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.commandBufferAllocation->getUnderlyingBufferSize()),
               batchBuffer.commandBufferAllocation->getUnderlyingBufferSize());
        printf("Command buffer - start gpu address: %" PRIx64 " - %" PRIx64 ", start cpu address: %p - %p, start offset: %zu, used size: %zu \n",
               ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset),
               ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.usedSize),
               ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.startOffset),
               ptrOffset(batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), batchBuffer.usedSize),
               batchBuffer.startOffset,
               batchBuffer.usedSize);
        printf("Ring buffer for submission - start gpu address: %" PRIx64 " - %" PRIx64 ", start cpu address: %p - %p, size: %zu,  submission address: %" PRIx64 ", used size: %zu, copyCmdBuffer: %d \n",
               ringCommandStream.getGraphicsAllocation()->getGpuAddress(),
               ptrOffset(ringCommandStream.getGraphicsAllocation()->getGpuAddress(), ringCommandStream.getGraphicsAllocation()->getUnderlyingBufferSize()),
               ringCommandStream.getGraphicsAllocation()->getUnderlyingBuffer(),
               ptrOffset(ringCommandStream.getGraphicsAllocation()->getUnderlyingBuffer(), ringCommandStream.getGraphicsAllocation()->getUnderlyingBufferSize()),
               ringCommandStream.getGraphicsAllocation()->getUnderlyingBufferSize(),
               ptrOffset(ringCommandStream.getGraphicsAllocation()->getGpuAddress(), ringCommandStream.getUsed()),
               ringCommandStream.getUsed(),
               copyCmdBuffer);
    }

    if (batchBuffer.pagingFenceSemInfo.requiresProgrammingSemaphore()) {
        dispatchSemaphoreForPagingFence(batchBuffer.pagingFenceSemInfo.pagingFenceValue);
    }

    auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);
    void *returnCmd = batchBuffer.endCmdPtr;

    LinearStream relaxedOrderingReturnPtrCmdStream;
    if (this->relaxedOrderingEnabled && batchBuffer.hasRelaxedOrderingDependencies) {
        // preallocate and patch after start section
        auto relaxedOrderingReturnPtrCmds = ringCommandStream.getSpace(RelaxedOrderingHelper::getSizeReturnPtrRegs<GfxFamily>());
        relaxedOrderingReturnPtrCmdStream.replaceBuffer(relaxedOrderingReturnPtrCmds, RelaxedOrderingHelper::getSizeReturnPtrRegs<GfxFamily>());
    }

    if (copyCmdBuffer) {
        auto cmdStreamTaskPtr = ptrOffset(batchBuffer.stream->getCpuBase(), batchBuffer.startOffset);
        auto sizeToCopy = ptrDiff(returnCmd, cmdStreamTaskPtr);
        auto ringPtr = ringCommandStream.getSpace(sizeToCopy);
        memcpy(ringPtr, cmdStreamTaskPtr, sizeToCopy);
    } else {
        dispatchStartSection(commandStreamAddress);
    }

    uint64_t returnGpuPointer = ringCommandStream.getCurrentGpuAddressPosition();

    if (this->relaxedOrderingEnabled && batchBuffer.hasRelaxedOrderingDependencies) {
        dispatchRelaxedOrderingReturnPtrRegs(relaxedOrderingReturnPtrCmdStream, returnGpuPointer);
    } else if (!copyCmdBuffer) {
        setReturnAddress(returnCmd, returnGpuPointer);
    }

    if (this->relaxedOrderingEnabled && batchBuffer.hasRelaxedOrderingDependencies) {
        dispatchTaskStoreSection(batchBuffer.taskStartAddress);

        uint32_t expectedQueueSize = batchBuffer.numCsrClients * RelaxedOrderingHelper::queueSizeMultiplier;
        expectedQueueSize = std::min(expectedQueueSize, RelaxedOrderingHelper::maxQueueSize);

        if (expectedQueueSize > this->currentRelaxedOrderingQueueSize && debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.get() == -1) {
            updateRelaxedOrderingQueueSize(expectedQueueSize);
        }
    }

    if (!disableCacheFlush) {
        Dispatcher::dispatchCacheFlush(ringCommandStream, this->rootDeviceEnvironment, gpuVaForMiFlush);
    }

    if (dispatchMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
    }

    dispatchSemaphoreSection(currentQueueWorkCount + 1);
    return currentPosition;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::copyCommandBufferIntoRing(BatchBuffer &batchBuffer) {
    /* Command buffer can't be copied into ring if implicit scaling or metrics are enabled,
       because those features uses GPU VAs of command buffer which would be invalid after copy. */

    auto ret = !batchBuffer.disableFlatRingBuffer &&
               this->osContext.getNumSupportedDevices() == 1u &&
               !this->rootDeviceEnvironment.executionEnvironment.areMetricsEnabled() &&
               !batchBuffer.chainedBatchBuffer &&
               batchBuffer.commandBufferAllocation &&
               MemoryPoolHelper::isSystemMemoryPool(batchBuffer.commandBufferAllocation->getMemoryPool()) &&
               !batchBuffer.hasRelaxedOrderingDependencies;

    if (debugManager.flags.DirectSubmissionFlatRingBuffer.get() != -1) {
        ret &= !!debugManager.flags.DirectSubmissionFlatRingBuffer.get();
    }

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getUllsStateSize() {
    size_t startSize = 0u;
    if (!this->partitionConfigSet) {
        startSize += getSizePartitionRegisterConfigurationSection();
    }
    if (this->miMemFenceRequired && !this->systemMemoryFenceAddressSet) {
        startSize += getSizeSystemMemoryFenceAddress();
    }
    if (this->relaxedOrderingEnabled && !this->relaxedOrderingInitialized) {
        startSize += RelaxedOrderingHelper::getSizeRegistersInit<GfxFamily>();
    }
    return startSize;
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchUllsState() {
    if (!this->partitionConfigSet) {
        dispatchPartitionRegisterConfiguration();
        this->partitionConfigSet = true;
    }
    if (this->globalFenceAllocation && !this->systemMemoryFenceAddressSet) {
        dispatchSystemMemoryFenceAddress();
        this->systemMemoryFenceAddressSet = true;
    }
    if (this->relaxedOrderingEnabled && !this->relaxedOrderingInitialized) {
        preinitializeRelaxedOrderingSections();
        dispatchStaticRelaxedOrderingScheduler();
        initRelaxedOrderingRegisters();

        this->relaxedOrderingInitialized = true;
    }
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp) {
    this->handleRingRestartForUllsLightResidency(batchBuffer.allocationsForResidency);

    lastSubmittedThrottle = batchBuffer.throttle;
    bool relaxedOrderingSchedulerWillBeNeeded = (this->relaxedOrderingSchedulerRequired || batchBuffer.hasRelaxedOrderingDependencies);
    bool inputRequiredMonitorFence = false;
    if (this->inputMonitorFenceDispatchRequirement) {
        inputRequiredMonitorFence = batchBuffer.dispatchMonitorFence;
    } else {
        inputRequiredMonitorFence = batchBuffer.hasStallingCmds;
    }
    bool dispatchMonitorFence = this->dispatchMonitorFenceRequired(inputRequiredMonitorFence);

    size_t dispatchSize = this->getUllsStateSize() + getSizeDispatch(relaxedOrderingSchedulerWillBeNeeded, batchBuffer.hasRelaxedOrderingDependencies, dispatchMonitorFence);

    if (this->copyCommandBufferIntoRing(batchBuffer)) {
        dispatchSize += (batchBuffer.stream->getUsed() - batchBuffer.startOffset) - 2 * getSizeStartSection();
    }
    if (batchBuffer.pagingFenceSemInfo.requiresProgrammingSemaphore()) {
        dispatchSize += getSizeSemaphoreForPagingFence();
    }

    size_t cycleSize = getSizeSwitchRingBufferSection();
    size_t requiredMinimalSize = dispatchSize + cycleSize + getSizeEnd(relaxedOrderingSchedulerWillBeNeeded);
    if (this->relaxedOrderingEnabled) {
        requiredMinimalSize += +RelaxedOrderingHelper::getSizeReturnPtrRegs<GfxFamily>();

        if (batchBuffer.hasStallingCmds && this->relaxedOrderingSchedulerRequired) {
            requiredMinimalSize += getSizeDispatchRelaxedOrderingQueueStall();
        }
        if (batchBuffer.hasRelaxedOrderingDependencies) {
            requiredMinimalSize += RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>() + sizeof(typename GfxFamily::MI_STORE_DATA_IMM);
        }
    }

    auto needStart = !this->ringStart;

    this->switchRingBuffersNeeded(requiredMinimalSize, batchBuffer.allocationsForResidency);

    auto startVA = ringCommandStream.getCurrentGpuAddressPosition();

    this->dispatchUllsState();

    if (this->relaxedOrderingEnabled && batchBuffer.hasStallingCmds && this->relaxedOrderingSchedulerRequired) {
        dispatchRelaxedOrderingQueueStall();
    }

    this->relaxedOrderingSchedulerRequired |= batchBuffer.hasRelaxedOrderingDependencies;

    handleNewResourcesSubmission();

    void *currentPosition = dispatchWorkloadSection(batchBuffer, dispatchMonitorFence);

    cpuCachelineFlush(currentPosition, dispatchSize);

    auto requiresBlockingResidencyHandling = batchBuffer.pagingFenceSemInfo.requiresBlockingResidencyHandling;
    if (!this->submitCommandBufferToGpu(needStart, startVA, requiredMinimalSize, requiresBlockingResidencyHandling, batchBuffer.allocationsForResidency)) {
        return false;
    }

    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);
    currentQueueWorkCount++;

    uint64_t flushValue = updateTagValue(dispatchMonitorFence);
    if (flushValue == DirectSubmissionHw<GfxFamily, Dispatcher>::updateTagValueFail) {
        return false;
    }
    flushStamp.setStamp(flushValue);

    return this->ringStart;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::submitCommandBufferToGpu(bool needStart, uint64_t gpuAddress, size_t size, bool needWait, const ResidencyContainer *allocationsForResidency) {
    if (needStart) {
        this->ringStart = this->submit(gpuAddress, size, allocationsForResidency);
        return this->ringStart;
    } else {
        if (needWait) {
            handleResidency();
        }
        this->unblockGpu();
        return true;
    }
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
    if (isNewResourceHandleNeeded()) {
        auto tlbFlushCounter = this->osContext.peekTlbFlushCounter();
        Dispatcher::dispatchTlbFlush(this->ringCommandStream, this->gpuVaForMiFlush, this->rootDeviceEnvironment);
        this->osContext.setTlbFlushed(tlbFlushCounter);
    }
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeNewResourceHandler() {
    // Overestimate to avoid race
    return Dispatcher::getSizeTlbFlush(this->rootDeviceEnvironment);
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::isNewResourceHandleNeeded() {
    auto newResourcesBound = this->osContext.isTlbFlushRequired();

    if (debugManager.flags.DirectSubmissionNewResourceTlbFlush.get() != -1) {
        newResourcesBound = debugManager.flags.DirectSubmissionNewResourceTlbFlush.get();
    }

    return newResourcesBound;
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffersNeeded(size_t size, ResidencyContainer *allocationsForResidency) {
    if (this->ringCommandStream.getAvailableSpace() < size) {
        this->switchRingBuffers(allocationsForResidency);
    }
}

template <typename GfxFamily, typename Dispatcher>
inline uint64_t DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffers(ResidencyContainer *allocationsForResidency) {
    GraphicsAllocation *nextRingBuffer = switchRingBuffersAllocations(allocationsForResidency);

    this->handleRingRestartForUllsLightResidency(allocationsForResidency);

    void *flushPtr = ringCommandStream.getSpace(0);
    uint64_t currentBufferGpuVa = ringCommandStream.getCurrentGpuAddressPosition();

    if (ringStart) {
        dispatchSwitchRingBufferSection(nextRingBuffer->getGpuAddress());
        cpuCachelineFlush(flushPtr, getSizeSwitchRingBufferSection());
    }

    ringCommandStream.replaceBuffer(nextRingBuffer->getUnderlyingBuffer(), ringCommandStream.getMaxAvailableSpace());
    ringCommandStream.replaceGraphicsAllocation(nextRingBuffer);

    handleSwitchRingBuffers(allocationsForResidency);

    return currentBufferGpuVa;
}

template <typename GfxFamily, typename Dispatcher>
inline GraphicsAllocation *DirectSubmissionHw<GfxFamily, Dispatcher>::switchRingBuffersAllocations(ResidencyContainer *allocationsForResidency) {
    this->previousRingBuffer = this->currentRingBuffer;
    GraphicsAllocation *nextAllocation = nullptr;
    for (uint32_t ringBufferIndex = 0; ringBufferIndex < this->ringBuffers.size(); ringBufferIndex++) {
        if (ringBufferIndex != this->currentRingBuffer && this->isCompleted(ringBufferIndex)) {
            this->currentRingBuffer = ringBufferIndex;
            nextAllocation = this->ringBuffers[ringBufferIndex].ringBuffer;
            break;
        }
    }

    if (nextAllocation == nullptr) {
        if (this->ringBuffers.size() == this->maxRingBufferCount) {
            this->currentRingBuffer = (this->currentRingBuffer + 1) % this->ringBuffers.size();
            nextAllocation = this->ringBuffers[this->currentRingBuffer].ringBuffer;
        } else {
            bool isMultiOsContextCapable = osContext.getNumSupportedDevices() > 1u;
            constexpr size_t minimumRequiredSize = 256 * MemoryConstants::kiloByte;
            constexpr size_t additionalAllocationSize = MemoryConstants::pageSize;
            const auto allocationSize = alignUp(minimumRequiredSize + additionalAllocationSize, MemoryConstants::pageSize64k);
            const AllocationProperties commandStreamAllocationProperties{rootDeviceIndex,
                                                                         true, allocationSize,
                                                                         AllocationType::ringBuffer,
                                                                         isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
            nextAllocation = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
            this->currentRingBuffer = static_cast<uint32_t>(this->ringBuffers.size());
            this->ringBuffers.emplace_back(0ull, 0ull, nextAllocation);
            auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(&nextAllocation, 1u), false, false, false) == MemoryOperationsStatus::success;
            UNRECOVERABLE_IF(!ret);

            this->handleResidencyContainerForUllsLightNewRingAllocation(allocationsForResidency);
        }
    }
    UNRECOVERABLE_IF(this->currentRingBuffer == this->previousRingBuffer);
    return nextAllocation;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchMonitorFenceRequired(bool requireMonitorFence) {
    return !this->disableMonitorFence;
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::deallocateResources() {
    for (uint32_t ringBufferIndex = 0; ringBufferIndex < this->ringBuffers.size(); ringBufferIndex++) {
        memoryManager->freeGraphicsMemory(this->ringBuffers[ringBufferIndex].ringBuffer);
    }
    this->ringBuffers.clear();
    if (semaphores) {
        memoryManager->freeGraphicsMemory(semaphores);
        semaphores = nullptr;
    }

    memoryManager->freeGraphicsMemory(deferredTasksListAllocation);
    memoryManager->freeGraphicsMemory(relaxedOrderingSchedulerAllocation);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSystemMemoryFenceAddress() {
    this->makeGlobalFenceAlwaysResident();
    EncodeMemoryFence<GfxFamily>::encodeSystemMemoryFence(ringCommandStream, this->globalFenceAllocation);
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSystemMemoryFenceAddress() {
    return EncodeMemoryFence<GfxFamily>::getSystemMemoryFenceSize();
}

template <typename GfxFamily, typename Dispatcher>
uint32_t DirectSubmissionHw<GfxFamily, Dispatcher>::getDispatchErrorCode() {
    return dispatchErrorCode;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSemaphoreForPagingFence(uint64_t value) {
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                          this->gpuVaForPagingFenceSemaphore,
                                                          value,
                                                          COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, false, nullptr);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSemaphoreForPagingFence() {
    return EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
}

} // namespace NEO
