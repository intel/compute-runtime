/*
 * Copyright (C) 2020-2022 Intel Corporation
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
#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/source/utilities/cpuintrinsics.h"

#include "create_direct_submission_hw.inl"

#include <cstring>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DirectSubmissionHw<GfxFamily, Dispatcher>::DirectSubmissionHw(const DirectSubmissionInputParams &inputParams)
    : ringBuffers(RingBufferUse::initialRingBufferCount), osContext(inputParams.osContext), rootDeviceIndex(inputParams.rootDeviceIndex) {
    memoryManager = inputParams.memoryManager;
    globalFenceAllocation = inputParams.globalFenceAllocation;
    logicalStateHelper = inputParams.logicalStateHelper;
    hwInfo = inputParams.rootDeviceEnvironment.getHardwareInfo();
    memoryOperationHandler = inputParams.rootDeviceEnvironment.memoryOperationsInterface.get();

    auto hwInfoConfig = HwInfoConfig::get(hwInfo->platform.eProductFamily);

    disableCacheFlush = UllsDefaults::defaultDisableCacheFlush;
    disableMonitorFence = UllsDefaults::defaultDisableMonitorFence;

    if (DebugManager.flags.DirectSubmissionMaxRingBuffers.get() != -1) {
        this->maxRingBufferCount = DebugManager.flags.DirectSubmissionMaxRingBuffers.get();
    }

    if (DebugManager.flags.DirectSubmissionDisableCacheFlush.get() != -1) {
        disableCacheFlush = !!DebugManager.flags.DirectSubmissionDisableCacheFlush.get();
    }

    miMemFenceRequired = hwInfoConfig->isGlobalFenceInDirectSubmissionRequired(*hwInfo);
    if (DebugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() == 0) {
        miMemFenceRequired = false;
    }
    if (DebugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.get() != -1) {
        sfenceMode = static_cast<DirectSubmissionSfenceMode>(DebugManager.flags.DirectSubmissionInsertSfenceInstructionPriorToSubmission.get());
    }

    int32_t disableCacheFlushKey = DebugManager.flags.DirectSubmissionDisableCpuCacheFlush.get();
    if (disableCacheFlushKey != -1) {
        disableCpuCacheFlush = disableCacheFlushKey == 1 ? true : false;
    }

    isDisablePrefetcherRequired = hwInfoConfig->isPrefetcherDisablingInDirectSubmissionRequired();
    if (DebugManager.flags.DirectSubmissionDisablePrefetcher.get() != -1) {
        isDisablePrefetcherRequired = !!DebugManager.flags.DirectSubmissionDisablePrefetcher.get();
    }

    UNRECOVERABLE_IF(!CpuInfo::getInstance().isFeatureSupported(CpuInfo::featureClflush) && !disableCpuCacheFlush);

    createDiagnostic();
    setPostSyncOffset();

    dcFlushRequired = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, *hwInfo);
    relaxedOrderingEnabled = (DebugManager.flags.DirectSubmissionRelaxedOrdering.get() == 1);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingSchedulerSection(uint32_t value) {
    uint64_t schedulerStartAddress = getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));
    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();

    // 1. Init section
    {
        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);
        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(ringCommandStream,
                                                                                          schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionStart,
                                                                                          CS_GPR_R1, 0, CompareOperation::Equal, false);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R2, 0, true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R2 + 4, 0, true);

        uint64_t removeTaskVa = schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::removeTaskSectionStart;
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R3, static_cast<uint32_t>(removeTaskVa & 0xFFFF'FFFFULL), true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R3 + 4, static_cast<uint32_t>(removeTaskVa >> 32), true);

        uint64_t walkersLoopConditionCheckVa = schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::tasksListLoopCheckSectionStart;
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R4, static_cast<uint32_t>(walkersLoopConditionCheckVa & 0xFFFF'FFFFULL), true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R4 + 4, static_cast<uint32_t>(walkersLoopConditionCheckVa >> 32), true);
    }

    // 2. Dispatch task section (loop start)
    {
        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R6, 8, true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R6 + 4, 0, true);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true);

        EncodeAluHelper<GfxFamily, 10> aluHelper;
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_2);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_6);
        aluHelper.setNextAlu(AluRegisters::OPCODE_SHL);
        aluHelper.setNextAlu(AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_ACCU);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_7);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8);
        aluHelper.setNextAlu(AluRegisters::OPCODE_ADD);
        aluHelper.setNextAlu(AluRegisters::OPCODE_STORE, AluRegisters::R_6, AluRegisters::R_ACCU);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOADIND, AluRegisters::R_0, AluRegisters::R_ACCU);
        aluHelper.setNextAlu(AluRegisters::OPCODE_FENCE_RD);

        aluHelper.copyToCmdStream(ringCommandStream);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&ringCommandStream, 0, false, true, false);
    }

    // 3. Remove task section
    {
        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);

        EncodeMathMMIO<GfxFamily>::encodeDecrement(ringCommandStream, AluRegisters::R_1);
        EncodeMathMMIO<GfxFamily>::encodeDecrement(ringCommandStream, AluRegisters::R_2);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(ringCommandStream,
                                                                                          schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionStart,
                                                                                          CS_GPR_R1, 0, CompareOperation::Equal, false);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R7, 8, true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R7 + 4, 0, true);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true);

        EncodeAluHelper<GfxFamily, 14> aluHelper;
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_1);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_7);
        aluHelper.setNextAlu(AluRegisters::OPCODE_SHL);
        aluHelper.setNextAlu(AluRegisters::OPCODE_STORE, AluRegisters::R_7, AluRegisters::R_ACCU);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_7);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8);
        aluHelper.setNextAlu(AluRegisters::OPCODE_ADD);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOADIND, AluRegisters::R_7, AluRegisters::R_ACCU);
        aluHelper.setNextAlu(AluRegisters::OPCODE_FENCE_RD);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_6);
        aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD0, AluRegisters::R_SRCB, AluRegisters::OPCODE_NONE);
        aluHelper.setNextAlu(AluRegisters::OPCODE_ADD);
        aluHelper.setNextAlu(AluRegisters::OPCODE_STOREIND, AluRegisters::R_ACCU, AluRegisters::R_7);
        aluHelper.setNextAlu(AluRegisters::OPCODE_FENCE_WR);

        aluHelper.copyToCmdStream(ringCommandStream);
    }

    // 4. List loop check section
    {
        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);

        EncodeMathMMIO<GfxFamily>::encodeIncrement(ringCommandStream, AluRegisters::R_2);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalRegRegBatchBufferStart(ringCommandStream,
                                                                                         schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart,
                                                                                         AluRegisters::R_1, AluRegisters::R_2, CompareOperation::NotEqual, false);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R2, 0, true);
        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R2 + 4, 0, true);
    }

    // 5. Drain request section
    {
        *ringCommandStream.getSpaceForCmd<typename GfxFamily::MI_ARB_CHECK>() = GfxFamily::cmdInitArbCheck;

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(ringCommandStream,
                                                                                          schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart,
                                                                                          CS_GPR_R5, 1, CompareOperation::Equal, false);
    }

    // 6. Scheduler loop check section
    {
        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(ringCommandStream, schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::endSectionStart,
                                                                                          semaphoreGpuVa, value, CompareOperation::GreaterOrEqual, false);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&ringCommandStream, schedulerStartAddress + RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart, false, false, false);
    }

    // 7. Semaphore section
    {
        using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);

        EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                              semaphoreGpuVa,
                                                              value,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    }

    // 8. End section
    {
        EncodeMiPredicate<GfxFamily>::encode(ringCommandStream, MiPredicateType::Disable);

        LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R5, 0, true);
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
                                                                 AllocationType::RING_BUFFER,
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
                                                             AllocationType::SEMAPHORE_BUFFER,
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
                                                        AllocationType::DEFERRED_TASKS_LIST,
                                                        isMultiOsContextCapable, false, osContext.getDeviceBitfield());

        deferredTasksListAllocation = memoryManager->allocateGraphicsMemoryWithProperties(allocationProperties);
        UNRECOVERABLE_IF(deferredTasksListAllocation == nullptr);

        allocations.push_back(deferredTasksListAllocation);
    }

    if (DebugManager.flags.DirectSubmissionPrintBuffers.get()) {
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
    auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(allocations), false) == MemoryOperationsStatus::SUCCESS;

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::unblockGpu() {
    if (sfenceMode >= DirectSubmissionSfenceMode::BeforeSemaphoreOnly) {
        CpuIntrinsics::sfence();
    }

    semaphoreData->QueueWorkCount = currentQueueWorkCount;

    if (sfenceMode == DirectSubmissionSfenceMode::BeforeAndAfterSemaphore) {
        CpuIntrinsics::sfence();
    }
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
                                 getSizeSemaphoreSection(true);

        Dispatcher::dispatchPreemption(ringCommandStream);
        if (this->partitionedMode) {
            startBufferSize += getSizePartitionRegisterConfigurationSection();
            dispatchPartitionRegisterConfiguration();

            this->partitionConfigSet = true;
        }
        if (this->miMemFenceRequired) {
            startBufferSize += getSizeSystemMemoryFenceAddress();
            dispatchSystemMemoryFenceAddress();

            this->systemMemoryFenceAddressSet = true;
        }
        if (this->relaxedOrderingEnabled) {
            preinitializeTaskStoreSection();

            initRelaxedOrderingRegisters();
            startBufferSize += RelaxedOrderingHelper::getSizeRegistersInit<GfxFamily>();

            this->relaxedOrderingInitialized = true;
        }
        if (workloadMode == 1) {
            dispatchDiagnosticModeSection();
            startBufferSize += getDiagnosticModeSection();
        }
        dispatchSemaphoreSection(currentQueueWorkCount, true);

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

    size_t startSize = getSizeSemaphoreSection(true);
    if (!this->partitionConfigSet) {
        startSize += getSizePartitionRegisterConfigurationSection();
    }
    if (this->miMemFenceRequired && !this->systemMemoryFenceAddressSet) {
        startSize += getSizeSystemMemoryFenceAddress();
    }
    if (this->relaxedOrderingEnabled && !this->relaxedOrderingInitialized) {
        startSize += RelaxedOrderingHelper::getSizeRegistersInit<GfxFamily>();
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

    if (this->miMemFenceRequired && !this->systemMemoryFenceAddressSet) {
        dispatchSystemMemoryFenceAddress();
        this->systemMemoryFenceAddressSet = true;
    }

    if (this->relaxedOrderingEnabled && !this->relaxedOrderingInitialized) {
        preinitializeTaskStoreSection();
        initRelaxedOrderingRegisters();

        this->relaxedOrderingInitialized = true;
    }

    currentQueueWorkCount++;
    dispatchSemaphoreSection(currentQueueWorkCount, true);

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
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
    }
    Dispatcher::dispatchStopCommandBuffer(ringCommandStream);

    auto bytesToPad = Dispatcher::getSizeStartCommandBuffer() - Dispatcher::getSizeStopCommandBuffer();
    EncodeNoop<GfxFamily>::emitNoop(ringCommandStream, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(ringCommandStream);

    cpuCachelineFlush(flushPtr, getSizeEnd());
    this->unblockGpu();
    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);

    this->handleStopRingBuffer();
    this->ringStart = false;

    return true;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSemaphoreSection(uint32_t value, bool firstSubmission) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    dispatchDisablePrefetcher(true);

    if (this->relaxedOrderingEnabled && !firstSubmission) {
        dispatchRelaxedOrderingSchedulerSection(value);
    } else {
        EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(ringCommandStream,
                                                              semaphoreGpuVa,
                                                              value,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    }

    if (miMemFenceRequired) {
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronizationForDirectSubmission(ringCommandStream, this->gpuVaForAdditionalSynchronizationWA, true, *hwInfo);
    }

    dispatchPrefetchMitigation();
    dispatchDisablePrefetcher(false);
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSemaphoreSection(bool firstSubmission) {
    size_t semaphoreSize = (this->relaxedOrderingEnabled && !firstSubmission) ? RelaxedOrderingHelper::SchedulerSizeAndOffsetSection<GfxFamily>::totalSize
                                                                              : EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait();
    semaphoreSize += getSizePrefetchMitigation();

    if (isDisablePrefetcherRequired) {
        semaphoreSize += 2 * getSizeDisablePrefetcher();
    }

    if (miMemFenceRequired) {
        semaphoreSize += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronizationForDirectSubmission(*hwInfo);
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
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
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
    size_t size = getSizeSemaphoreSection(false);
    if (workloadMode == 0) {
        size += getSizeStartSection();
    } else if (workloadMode == 1) {
        size += getDiagnosticModeSection();
    }
    // mode 2 does not dispatch any commands

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
    // mode 2 does not dispatch any commands

    if (this->relaxedOrderingEnabled) {
        dispatchTaskStoreSection(batchBuffer.taskStartAddress);
    }

    if (!disableCacheFlush) {
        Dispatcher::dispatchCacheFlush(ringCommandStream, *hwInfo, gpuVaForMiFlush);
    }

    if (!disableMonitorFence) {
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, *hwInfo,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
    }

    dispatchSemaphoreSection(currentQueueWorkCount + 1, false);
    return currentPosition;
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::initRelaxedOrderingRegisters() {
    LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R1, 0, true);
    LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R1 + 4, 0, true);
    LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R5, 0, true);
    LriHelper<GfxFamily>::program(&ringCommandStream, CS_GPR_R5 + 4, 0, true);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::preinitializeTaskStoreSection() {
    preinitializedTaskStoreSection = std::make_unique<uint8_t[]>(RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    LinearStream stream(preinitializedTaskStoreSection.get(), RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    EncodeMiPredicate<GfxFamily>::encode(stream, MiPredicateType::Disable);

    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R6, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true);
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R6 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true);

    // Task start VA
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R7, 0, true);
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R7 + 4, 0, true);

    // Shift by 8 = multiply by 256. Address must by 64b aligned (shift by 6), but SHL accepts only 1, 2, 4, 8, 16 and 32
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R8, 8, true);
    LriHelper<GfxFamily>::program(&stream, CS_GPR_R8 + 4, 0, true);

    EncodeAluHelper<GfxFamily, 9> aluHelper;
    aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_1);
    aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_8);
    aluHelper.setNextAlu(AluRegisters::OPCODE_SHL);
    aluHelper.setNextAlu(AluRegisters::OPCODE_STORE, AluRegisters::R_8, AluRegisters::R_ACCU);
    aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCA, AluRegisters::R_8);
    aluHelper.setNextAlu(AluRegisters::OPCODE_LOAD, AluRegisters::R_SRCB, AluRegisters::R_6);
    aluHelper.setNextAlu(AluRegisters::OPCODE_ADD);
    aluHelper.setNextAlu(AluRegisters::OPCODE_STOREIND, AluRegisters::R_ACCU, AluRegisters::R_7);
    aluHelper.setNextAlu(AluRegisters::OPCODE_FENCE_WR);

    aluHelper.copyToCmdStream(stream);

    EncodeMathMMIO<GfxFamily>::encodeIncrement(stream, AluRegisters::R_1);

    UNRECOVERABLE_IF(stream.getUsed() != RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchTaskStoreSection(uint64_t taskStartSectionVa) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    constexpr size_t patchOffset = EncodeMiPredicate<GfxFamily>::getCmdSize() + (2 * sizeof(MI_LOAD_REGISTER_IMM));

    auto lri = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ptrOffset(preinitializedTaskStoreSection.get(), patchOffset));

    lri->setDataDword(static_cast<uint32_t>(taskStartSectionVa & 0xFFFF'FFFFULL));
    lri++;
    lri->setDataDword(static_cast<uint32_t>(taskStartSectionVa >> 32));

    auto dst = ringCommandStream.getSpace(RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());
    memcpy_s(dst, RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>(), preinitializedTaskStoreSection.get(), RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchCommandBuffer(BatchBuffer &batchBuffer, FlushStampTracker &flushStamp) {
    // for now workloads requiring cache coherency are not supported
    UNRECOVERABLE_IF(batchBuffer.requiresCoherency);

    if (batchBuffer.ringBufferRestartRequest) {
        this->stopRingBuffer();
    }

    this->startRingBuffer();

    size_t dispatchSize = getSizeDispatch();
    size_t cycleSize = getSizeSwitchRingBufferSection();
    size_t requiredMinimalSize = dispatchSize + cycleSize + getSizeEnd();
    if (this->relaxedOrderingEnabled) {
        requiredMinimalSize += RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>();
    }

    getCommandBufferPositionGpuAddress(ringCommandStream.getSpace(0));

    if (ringCommandStream.getAvailableSpace() < requiredMinimalSize) {
        switchRingBuffers();
    }

    handleNewResourcesSubmission();

    void *currentPosition = dispatchWorkloadSection(batchBuffer);

    cpuCachelineFlush(currentPosition, dispatchSize);
    handleResidency();

    if (DebugManager.flags.DirectSubmissionReadBackCommandBuffer.get() == 1) {
        volatile auto cmdBufferStart = reinterpret_cast<uint32_t *>(batchBuffer.commandBufferAllocation->getUnderlyingBuffer());
        reserved = *cmdBufferStart;
    }

    if (DebugManager.flags.DirectSubmissionReadBackRingBuffer.get() == 1) {
        volatile auto ringBufferStart = reinterpret_cast<uint32_t *>(ringCommandStream.getSpace(0));
        reserved = *ringBufferStart;
    }

    this->unblockGpu();

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
                                                                         AllocationType::RING_BUFFER,
                                                                         isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
            nextAllocation = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
            this->currentRingBuffer = static_cast<uint32_t>(this->ringBuffers.size());
            this->ringBuffers.emplace_back(0ull, nextAllocation);
            auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(&nextAllocation, 1u), false) == MemoryOperationsStatus::SUCCESS;
            UNRECOVERABLE_IF(!ret);
        }
    }
    UNRECOVERABLE_IF(this->currentRingBuffer == this->previousRingBuffer);
    return nextAllocation;
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

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchSystemMemoryFenceAddress() {
    EncodeMemoryFence<GfxFamily>::encodeSystemMemoryFence(ringCommandStream, this->globalFenceAllocation, this->logicalStateHelper);

    if (logicalStateHelper) {
        logicalStateHelper->writeStreamInline(ringCommandStream, false);
    }
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeSystemMemoryFenceAddress() {
    return EncodeMemoryFence<GfxFamily>::getSystemMemoryFenceSize();
}

} // namespace NEO
