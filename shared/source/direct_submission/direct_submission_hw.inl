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
#include "shared/source/direct_submission/direct_submission_hw_diagnostic_mode.h"
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
#include "shared/source/utilities/cpuintrinsics.h"

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

    if (hwInfo->capabilityTable.isIntegratedDevice) {
        miMemFenceRequired = false;
    } else {
        miMemFenceRequired = productHelper.isGlobalFenceInDirectSubmissionRequired(*hwInfo);
    }

    if (debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get() != -1) {
        miMemFenceRequired = debugManager.flags.DirectSubmissionInsertExtraMiMemFenceCommands.get();
    }

    if (miMemFenceRequired && compilerProductHelper.isHeaplessStateInitEnabled(compilerProductHelper.isHeaplessModeEnabled())) {
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

    createDiagnostic();
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
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchStaticRelaxedOrderingScheduler() {
    LinearStream schedulerCmdStream(this->relaxedOrderingSchedulerAllocation);
    uint64_t schedulerStartAddress = schedulerCmdStream.getGpuBase();
    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();

    uint64_t loopSectionStartAddress = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart;

    const uint32_t miMathMocs = this->rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

    constexpr bool isBcs = Dispatcher::isCopy();

    // 1. Init section
    {
        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9, isBcs);
        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(schedulerCmdStream, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2, 0, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2 + 4, 0, true, isBcs);

        uint64_t removeTaskVa = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::removeTaskSectionStart;
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR3, static_cast<uint32_t>(removeTaskVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(removeTaskVa >> 32), true, isBcs);

        uint64_t walkersLoopConditionCheckVa = schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::tasksListLoopCheckSectionStart;
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR4, static_cast<uint32_t>(walkersLoopConditionCheckVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(walkersLoopConditionCheckVa >> 32), true, isBcs);
    }

    // 2. Dispatch task section (loop start)
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR6, 8, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR6 + 4, 0, true, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

        EncodeAluHelper<GfxFamily, 10> aluHelper;
        aluHelper.setMocs(miMathMocs);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr2);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6);
        aluHelper.setNextAlu(AluRegisters::opcodeShl);
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8);
        aluHelper.setNextAlu(AluRegisters::opcodeAdd);
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr6, AluRegisters::accu);
        aluHelper.setNextAlu(AluRegisters::opcodeLoadind, AluRegisters::gpr0, AluRegisters::accu);
        aluHelper.setNextAlu(AluRegisters::opcodeFenceRd);

        aluHelper.copyToCmdStream(schedulerCmdStream);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerCmdStream, 0, false, true, false);
    }

    // 3. Remove task section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::removeTaskSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeMathMMIO<GfxFamily>::encodeDecrement(schedulerCmdStream, AluRegisters::gpr1, isBcs);
        EncodeMathMMIO<GfxFamily>::encodeDecrement(schedulerCmdStream, AluRegisters::gpr2, isBcs);

        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR9, isBcs);
        EncodeSetMMIO<GfxFamily>::encodeREG(schedulerCmdStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR9 + 4, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(schedulerCmdStream, 0, RegisterOffsets::csGprR1, 0, CompareOperation::equal, true, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR7, 8, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR8 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

        EncodeAluHelper<GfxFamily, 14> aluHelper;
        aluHelper.setMocs(miMathMocs);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr7);
        aluHelper.setNextAlu(AluRegisters::opcodeShl);
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr7, AluRegisters::accu);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr7);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8);
        aluHelper.setNextAlu(AluRegisters::opcodeAdd);
        aluHelper.setNextAlu(AluRegisters::opcodeLoadind, AluRegisters::gpr7, AluRegisters::accu);
        aluHelper.setNextAlu(AluRegisters::opcodeFenceRd);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr6);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad0, AluRegisters::srcb, AluRegisters::opcodeNone);
        aluHelper.setNextAlu(AluRegisters::opcodeAdd);
        aluHelper.setNextAlu(AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7);
        aluHelper.setNextAlu(AluRegisters::opcodeFenceWr);

        aluHelper.copyToCmdStream(schedulerCmdStream);
    }

    // 4. List loop check section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::tasksListLoopCheckSectionStart);

        EncodeMiPredicate<GfxFamily>::encode(schedulerCmdStream, MiPredicateType::disable);

        EncodeMathMMIO<GfxFamily>::encodeIncrement(schedulerCmdStream, AluRegisters::gpr2, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalRegRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            AluRegisters::gpr1, AluRegisters::gpr2, CompareOperation::notEqual, false, isBcs);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2, 0, true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR2 + 4, 0, true, isBcs);
    }

    // 5. Drain request section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::drainRequestSectionStart);

        EncodeMiArbCheck<GfxFamily>::program(schedulerCmdStream, std::nullopt);

        if (debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.get() != -1) {
            currentRelaxedOrderingQueueSize = static_cast<uint32_t>(debugManager.flags.DirectSubmissionRelaxedOrderingQueueSizeLimit.get());
        }

        this->relaxedOrderingQueueSizeLimitValueVa = schedulerCmdStream.getCurrentGpuAddressPosition() + RelaxedOrderingHelper::getQueueSizeLimitValueOffset<GfxFamily>();

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            RegisterOffsets::csGprR1, currentRelaxedOrderingQueueSize, CompareOperation::greaterOrEqual, false, false, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(
            schedulerCmdStream,
            loopSectionStartAddress,
            RegisterOffsets::csGprR5, 1, CompareOperation::equal, false, false, isBcs);
    }

    // 6. Scheduler loop check section
    {
        UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::schedulerLoopCheckSectionStart);

        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR10, static_cast<uint32_t>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionSize), true, isBcs);
        LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR10 + 4, 0, true, isBcs);

        EncodeAluHelper<GfxFamily, 4> aluHelper;
        aluHelper.setMocs(miMathMocs);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr9);
        aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr10);
        aluHelper.setNextAlu(AluRegisters::opcodeAdd);
        aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr0, AluRegisters::accu);
        aluHelper.copyToCmdStream(schedulerCmdStream);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalRegMemBatchBufferStart(schedulerCmdStream, 0, semaphoreGpuVa, RegisterOffsets::csGprR11, CompareOperation::greaterOrEqual, true, isBcs);

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerCmdStream, schedulerStartAddress + RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::loopStartSectionStart,
                                                                        false, false, false);
    }

    UNRECOVERABLE_IF(schedulerCmdStream.getUsed() != RelaxedOrderingHelper::StaticSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingSchedulerSection(uint32_t value) {
    LinearStream schedulerCmdStream(this->preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);

    // 1. Init section

    uint64_t schedulerStartVa = ringCommandStream.getCurrentGpuAddressPosition();

    uint64_t semaphoreSectionVa = schedulerStartVa + RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::semaphoreSectionStart;

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR11, value, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR9, static_cast<uint32_t>(semaphoreSectionVa & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerCmdStream, RegisterOffsets::csGprR9 + 4, static_cast<uint32_t>(semaphoreSectionVa >> 32), true, isBcs);

    schedulerCmdStream.getSpace(sizeof(typename GfxFamily::MI_BATCH_BUFFER_START)); // skip patching

    // 2. Semaphore section
    {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        schedulerCmdStream.getSpace(EncodeMiPredicate<GfxFamily>::getCmdSize()); // skip patching

        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(schedulerCmdStream, semaphoreGpuVa, value,
                                                              COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, false, nullptr);
    }

    // skip patching End section

    auto dst = ringCommandStream.getSpace(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
    memcpy_s(dst, RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize,
             this->preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
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
    workloadModeOneStoreAddress = static_cast<volatile void *>(&semaphoreData->diagnosticModeCounter);
    *static_cast<volatile uint32_t *>(workloadModeOneStoreAddress) = 0u;

    this->gpuVaForMiFlush = this->semaphoreGpuVa + offsetof(RingSemaphoreData, miFlushSpace);
    this->gpuVaForPagingFenceSemaphore = this->semaphoreGpuVa + offsetof(RingSemaphoreData, pagingFenceCounter);
    auto ret = makeResourcesResident(allocations);

    return ret && allocateOsResources();
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::makeResourcesResident(DirectSubmissionAllocations &allocations) {
    auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(allocations), false, false) == MemoryOperationsStatus::success;

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
inline void DirectSubmissionHw<GfxFamily, Dispatcher>::unblockGpu() {
    if (sfenceMode >= DirectSubmissionSfenceMode::beforeSemaphoreOnly) {
        CpuIntrinsics::sfence();
    }

    if (this->pciBarrierPtr) {
        *this->pciBarrierPtr = 0u;
    }

    if (debugManager.flags.DirectSubmissionPrintSemaphoreUsage.get() == 1) {
        printf("DirectSubmission semaphore %" PRIx64 " unlocked with value: %u\n", semaphoreGpuVa, currentQueueWorkCount);
    }

    semaphoreData->queueWorkCount = currentQueueWorkCount;

    if (sfenceMode == DirectSubmissionSfenceMode::beforeAndAfterSemaphore) {
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
                                 getSizeSemaphoreSection(false);

        Dispatcher::dispatchPreemption(ringCommandStream);
        if (this->partitionedMode) {
            startBufferSize += getSizePartitionRegisterConfigurationSection();
            dispatchPartitionRegisterConfiguration();

            this->partitionConfigSet = true;
        }
        if (this->miMemFenceRequired && !this->systemMemoryFenceAddressSet) {
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
        TagData currentTagData = {};
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
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
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronizationForDirectSubmission(ringCommandStream, this->gpuVaForAdditionalSynchronizationWA, true, rootDeviceEnvironment);
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
        getTagAddressValue(currentTagData);
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
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
        size += Dispatcher::getSizeMonitorFence(rootDeviceEnvironment);
    }
    if (this->relaxedOrderingEnabled && relaxedOrderingSchedulerRequired) {
        size += getSizeDispatchRelaxedOrderingQueueStall();
    }
    return size;
}

template <typename GfxFamily, typename Dispatcher>
inline size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDispatch(bool relaxedOrderingSchedulerRequired, bool returnPtrsRequired, bool dispatchMonitorFence) {
    size_t size = getSizeSemaphoreSection(relaxedOrderingSchedulerRequired);
    if (workloadMode == 0) {
        size += getSizeStartSection();
        if (this->relaxedOrderingEnabled && returnPtrsRequired) {
            size += RelaxedOrderingHelper::getSizeReturnPtrRegs<GfxFamily>();
        }
    } else if (workloadMode == 1) {
        size += getDiagnosticModeSection();
    }
    // mode 2 does not dispatch any commands

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
void DirectSubmissionHw<GfxFamily, Dispatcher>::updateRelaxedOrderingQueueSize(uint32_t newSize) {
    this->currentRelaxedOrderingQueueSize = newSize;

    EncodeStoreMemory<GfxFamily>::programStoreDataImm(this->ringCommandStream, this->relaxedOrderingQueueSizeLimitValueVa,
                                                      this->currentRelaxedOrderingQueueSize, 0, false, false,
                                                      nullptr);
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

    if (workloadMode == 0) {
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
    } else if (workloadMode == 1) {
        DirectSubmissionDiagnostics::diagnosticModeOneDispatch(diagnostic.get());
        dispatchDiagnosticModeSection();
    }
    // mode 2 does not dispatch any commands

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
        Dispatcher::dispatchMonitorFence(ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment,
                                         this->useNotifyForPostSync, this->partitionedMode, this->dcFlushRequired);
    }

    dispatchSemaphoreSection(currentQueueWorkCount + 1);
    return currentPosition;
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingQueueStall() {
    LinearStream bbStartStream(ringCommandStream.getSpace(EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false)),
                               EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false));

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5, 1, true, isBcs);
    dispatchSemaphoreSection(currentQueueWorkCount);

    // patch conditional bb_start with current GPU address
    EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataRegBatchBufferStart(bbStartStream, ringCommandStream.getCurrentGpuAddressPosition(),
                                                                                      RegisterOffsets::csGprR1, 0, CompareOperation::equal, false, false, isBcs);

    relaxedOrderingSchedulerRequired = false;
}

template <typename GfxFamily, typename Dispatcher>
size_t DirectSubmissionHw<GfxFamily, Dispatcher>::getSizeDispatchRelaxedOrderingQueueStall() {
    return getSizeSemaphoreSection(true) + sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) +
           EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataRegBatchBufferStart(false);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::dispatchRelaxedOrderingReturnPtrRegs(LinearStream &cmdStream, uint64_t returnPtr) {

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR4, static_cast<uint32_t>(returnPtr & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR4 + 4, static_cast<uint32_t>(returnPtr >> 32), true, isBcs);

    uint64_t returnPtrAfterTaskStoreSection = returnPtr;

    returnPtrAfterTaskStoreSection += RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>();

    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR3, static_cast<uint32_t>(returnPtrAfterTaskStoreSection & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&cmdStream, RegisterOffsets::csGprR3 + 4, static_cast<uint32_t>(returnPtrAfterTaskStoreSection >> 32), true, isBcs);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::initRelaxedOrderingRegisters() {

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR1, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR1 + 4, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&ringCommandStream, RegisterOffsets::csGprR5 + 4, 0, true, isBcs);
}

template <typename GfxFamily, typename Dispatcher>
void DirectSubmissionHw<GfxFamily, Dispatcher>::preinitializeRelaxedOrderingSections() {
    // Task store section
    preinitializedTaskStoreSection = std::make_unique<uint8_t[]>(RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    LinearStream stream(preinitializedTaskStoreSection.get(), RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    EncodeMiPredicate<GfxFamily>::encode(stream, MiPredicateType::disable);

    uint64_t deferredTasksListGpuVa = deferredTasksListAllocation->getGpuAddress();

    constexpr bool isBcs = Dispatcher::isCopy();
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR6, static_cast<uint32_t>(deferredTasksListGpuVa & 0xFFFF'FFFFULL), true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR6 + 4, static_cast<uint32_t>(deferredTasksListGpuVa >> 32), true, isBcs);

    // Task start VA
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR7, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR7 + 4, 0, true, isBcs);

    // Shift by 8 = multiply by 256. Address must by 64b aligned (shift by 6), but SHL accepts only 1, 2, 4, 8, 16 and 32
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR8, 8, true, isBcs);
    LriHelper<GfxFamily>::program(&stream, RegisterOffsets::csGprR8 + 4, 0, true, isBcs);

    const uint32_t miMathMocs = this->rootDeviceEnvironment.getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);

    EncodeAluHelper<GfxFamily, 9> aluHelper;
    aluHelper.setMocs(miMathMocs);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr1);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr8);
    aluHelper.setNextAlu(AluRegisters::opcodeShl);
    aluHelper.setNextAlu(AluRegisters::opcodeStore, AluRegisters::gpr8, AluRegisters::accu);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srca, AluRegisters::gpr8);
    aluHelper.setNextAlu(AluRegisters::opcodeLoad, AluRegisters::srcb, AluRegisters::gpr6);
    aluHelper.setNextAlu(AluRegisters::opcodeAdd);
    aluHelper.setNextAlu(AluRegisters::opcodeStoreind, AluRegisters::accu, AluRegisters::gpr7);
    aluHelper.setNextAlu(AluRegisters::opcodeFenceWr);

    aluHelper.copyToCmdStream(stream);

    EncodeMathMMIO<GfxFamily>::encodeIncrement(stream, AluRegisters::gpr1, isBcs);

    UNRECOVERABLE_IF(stream.getUsed() != RelaxedOrderingHelper::getSizeTaskStoreSection<GfxFamily>());

    // Scheduler section
    preinitializedRelaxedOrderingScheduler = std::make_unique<uint8_t[]>(RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
    LinearStream schedulerStream(preinitializedRelaxedOrderingScheduler.get(), RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);

    uint64_t schedulerStartAddress = relaxedOrderingSchedulerAllocation->getGpuAddress();

    // 1. Init section
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR11, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR9, 0, true, isBcs);
    LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR9 + 4, 0, true, isBcs);
    EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&schedulerStream, schedulerStartAddress, false, false, false);

    // 2. Semaphore section
    {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        EncodeMiPredicate<GfxFamily>::encode(schedulerStream, MiPredicateType::disable);

        EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(schedulerStream, 0, 0, COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD, false, false, false, false, nullptr);
    }

    // 3. End section
    {
        EncodeMiPredicate<GfxFamily>::encode(schedulerStream, MiPredicateType::disable);

        LriHelper<GfxFamily>::program(&schedulerStream, RegisterOffsets::csGprR5, 0, true, isBcs);
    }

    UNRECOVERABLE_IF(schedulerStream.getUsed() != RelaxedOrderingHelper::DynamicSchedulerSizeAndOffsetSection<GfxFamily>::totalSize);
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
    if (this->miMemFenceRequired && !this->systemMemoryFenceAddressSet) {
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
    if (!this->submitCommandBufferToGpu(needStart, startVA, requiredMinimalSize, requiresBlockingResidencyHandling)) {
        return false;
    }

    cpuCachelineFlush(semaphorePtr, MemoryConstants::cacheLineSize);
    currentQueueWorkCount++;
    DirectSubmissionDiagnostics::diagnosticModeOneSubmit(diagnostic.get());

    uint64_t flushValue = updateTagValue(dispatchMonitorFence);
    if (flushValue == DirectSubmissionHw<GfxFamily, Dispatcher>::updateTagValueFail) {
        return false;
    }
    flushStamp.setStamp(flushValue);

    return this->ringStart;
}

template <typename GfxFamily, typename Dispatcher>
bool DirectSubmissionHw<GfxFamily, Dispatcher>::submitCommandBufferToGpu(bool needStart, uint64_t gpuAddress, size_t size, bool needWait) {
    if (needStart) {
        this->ringStart = this->submit(gpuAddress, size);
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
    GraphicsAllocation *nextRingBuffer = switchRingBuffersAllocations();
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
                                                                         AllocationType::ringBuffer,
                                                                         isMultiOsContextCapable, false, osContext.getDeviceBitfield()};
            nextAllocation = memoryManager->allocateGraphicsMemoryWithProperties(commandStreamAllocationProperties);
            this->currentRingBuffer = static_cast<uint32_t>(this->ringBuffers.size());
            this->ringBuffers.emplace_back(0ull, nextAllocation);
            auto ret = memoryOperationHandler->makeResidentWithinOsContext(&this->osContext, ArrayRef<GraphicsAllocation *>(&nextAllocation, 1u), false, false) == MemoryOperationsStatus::success;
            UNRECOVERABLE_IF(!ret);
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
void DirectSubmissionHw<GfxFamily, Dispatcher>::createDiagnostic() {
    if (directSubmissionDiagnosticAvailable) {
        workloadMode = debugManager.flags.DirectSubmissionEnableDebugBuffer.get();
        if (workloadMode > 0) {
            disableCacheFlush = debugManager.flags.DirectSubmissionDisableCacheFlush.get();
            disableMonitorFence = debugManager.flags.DirectSubmissionDisableMonitorFence.get();
            uint32_t executions = static_cast<uint32_t>(debugManager.flags.DirectSubmissionDiagnosticExecutionCount.get());
            diagnostic = std::make_unique<DirectSubmissionDiagnosticsCollector>(
                executions,
                workloadMode == 1,
                debugManager.flags.DirectSubmissionBufferPlacement.get(),
                debugManager.flags.DirectSubmissionSemaphorePlacement.get(),
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
