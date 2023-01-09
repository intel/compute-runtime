/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/completion_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/bcs_split.h"
#include "level_zero/core/source/helpers/error_code_helper_l0.h"

#include "encode_surface_state_args.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::LogicalStateHelper *CommandListCoreFamilyImmediate<gfxCoreFamily>::getLogicalStateHelper() const {
    return this->csr->getLogicalStateHelper();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::checkAvailableSpace() {
    if (this->commandContainer.getCommandStream()->getAvailableSpace() < maxImmediateCommandSize) {

        auto alloc = this->commandContainer.reuseExistingCmdBuffer();
        this->commandContainer.addCurrentCommandBufferToReusableAllocationList();

        if (!alloc) {
            alloc = this->commandContainer.allocateCommandBuffer();
            this->commandContainer.getCmdBufferAllocations().push_back(alloc);
        }
        this->commandContainer.setCmdBuffer(alloc);
        this->cmdListCurrentStartOffset = 0;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::updateDispatchFlagsWithRequiredStreamState(NEO::DispatchFlags &dispatchFlags) {
    const auto &requiredFrontEndState = this->requiredStreamState.frontEndState;
    dispatchFlags.kernelExecutionType = (requiredFrontEndState.computeDispatchAllWalkerEnable.value == 1)
                                            ? NEO::KernelExecutionType::Concurrent
                                            : NEO::KernelExecutionType::Default;
    dispatchFlags.disableEUFusion = (requiredFrontEndState.disableEUFusion.value == 1);
    dispatchFlags.additionalKernelExecInfo = (requiredFrontEndState.disableOverdispatch.value == 1)
                                                 ? NEO::AdditionalKernelExecInfo::DisableOverdispatch
                                                 : NEO::AdditionalKernelExecInfo::NotSet;

    const auto &requiredStateComputeMode = this->requiredStreamState.stateComputeMode;
    dispatchFlags.requiresCoherency = (requiredStateComputeMode.isCoherencyRequired.value == 1);
    dispatchFlags.numGrfRequired = (requiredStateComputeMode.largeGrfMode.value == 1) ? GrfConfig::LargeGrfNumber
                                                                                      : GrfConfig::DefaultGrfNumber;
    dispatchFlags.threadArbitrationPolicy = requiredStateComputeMode.threadArbitrationPolicy.value;

    const auto &requiredPipelineSelect = this->requiredStreamState.pipelineSelect;
    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode = requiredPipelineSelect.systolicMode.value != -1
                                                                      ? !!requiredPipelineSelect.systolicMode.value
                                                                      : false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushBcsTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, NEO::CommandStreamReceiver *csr) {
    NEO::DispatchBcsFlags dispatchBcsFlags(
        this->isSyncModeQueue,         // flushTaskCount
        hasStallingCmds,               // hasStallingCmds
        hasRelaxedOrderingDependencies // hasRelaxedOrderingDependencies
    );

    return csr->flushBcsTask(cmdStreamTask, taskStartOffset, dispatchBcsFlags, this->device->getHwInfo());
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::CompletionStamp CommandListCoreFamilyImmediate<gfxCoreFamily>::flushRegularTask(NEO::LinearStream &cmdStreamTask, size_t taskStartOffset, bool hasStallingCmds, bool hasRelaxedOrderingDependencies) {
    NEO::DispatchFlags dispatchFlags(
        {},                                                          // csrDependencies
        nullptr,                                                     // barrierTimestampPacketNodes
        {},                                                          // pipelineSelectArgs
        nullptr,                                                     // flushStampReference
        NEO::QueueThrottle::MEDIUM,                                  // throttle
        this->getCommandListPreemptionMode(),                        // preemptionMode
        GrfConfig::NotApplicable,                                    // numGrfRequired
        NEO::L3CachingSettings::l3CacheOn,                           // l3CacheSettings
        NEO::ThreadArbitrationPolicy::NotPresent,                    // threadArbitrationPolicy
        NEO::AdditionalKernelExecInfo::NotApplicable,                // additionalKernelExecInfo
        NEO::KernelExecutionType::NotApplicable,                     // kernelExecutionType
        NEO::MemoryCompressionState::NotApplicable,                  // memoryCompressionState
        NEO::QueueSliceCount::defaultSliceCount,                     // sliceCount
        this->isSyncModeQueue,                                       // blocking
        this->isSyncModeQueue,                                       // dcFlush
        this->getCommandListSLMEnable(),                             // useSLM
        this->isSyncModeQueue,                                       // guardCommandBufferWithPipeControl
        false,                                                       // GSBA32BitRequired
        false,                                                       // requiresCoherency
        false,                                                       // lowPriority
        true,                                                        // implicitFlush
        this->csr->isNTo1SubmissionModelEnabled(),                   // outOfOrderExecutionAllowed
        false,                                                       // epilogueRequired
        false,                                                       // usePerDssBackedBuffer
        false,                                                       // useGlobalAtomics
        this->device->getNEODevice()->getNumGenericSubDevices() > 1, // areMultipleSubDevicesInContext
        false,                                                       // memoryMigrationRequired
        false,                                                       // textureCacheFlush
        hasStallingCmds,                                             // hasStallingCmds
        hasRelaxedOrderingDependencies                               // hasRelaxedOrderingDependencies
    );

    this->updateDispatchFlagsWithRequiredStreamState(dispatchFlags);
    this->csr->setRequiredScratchSizes(this->getCommandListPerThreadScratchSize(), this->getCommandListPerThreadPrivateScratchSize());

    auto ioh = (this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::INDIRECT_OBJECT));
    NEO::IndirectHeap *dsh = nullptr;
    NEO::IndirectHeap *ssh = nullptr;

    if (!NEO::ApiSpecificConfig::getBindlessConfiguration()) {
        dsh = (this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::DYNAMIC_STATE));
        ssh = (this->commandContainer.getIndirectHeap(NEO::IndirectHeap::Type::SURFACE_STATE));
    } else {
        dsh = this->device->getNEODevice()->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::BindlesHeapType::GLOBAL_DSH);
        ssh = this->device->getNEODevice()->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::BindlesHeapType::GLOBAL_SSH);
    }

    if (this->device->getL0Debugger()) {
        UNRECOVERABLE_IF(!NEO::Debugger::isDebugEnabled(this->internalUsage));
        this->csr->makeResident(*this->device->getL0Debugger()->getSbaTrackingBuffer(this->csr->getOsContext().getContextId()));
        this->csr->makeResident(*this->device->getDebugSurface());
    }

    NEO::Device *neoDevice = this->device->getNEODevice();
    if (neoDevice->getDebugger() && this->immediateCmdListHeapSharing) {
        auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
        auto sshStateCopy = csrHw->getSshState();
        bool sshDirty = sshStateCopy.updateAndCheck(ssh);

        if (sshDirty) {
            auto surfaceStateSpace = neoDevice->getDebugger()->getDebugSurfaceReservedSurfaceState(*ssh);
            auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

            NEO::EncodeSurfaceStateArgs args;
            args.outMemory = &surfaceState;
            args.graphicsAddress = this->device->getDebugSurface()->getGpuAddress();
            args.size = this->device->getDebugSurface()->getUnderlyingBufferSize();
            args.mocs = this->device->getMOCS(false, false);
            args.numAvailableDevices = neoDevice->getNumGenericSubDevices();
            args.allocation = this->device->getDebugSurface();
            args.gmmHelper = neoDevice->getGmmHelper();
            args.useGlobalAtomics = false;
            args.areMultipleSubDevicesInContext = false;
            args.isDebuggerActive = true;
            NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
            *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
        }
    }

    return this->csr->flushTask(
        cmdStreamTask,
        taskStartOffset,
        dsh,
        ioh,
        ssh,
        this->csr->peekTaskLevel(),
        dispatchFlags,
        *(this->device->getNEODevice()));
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::executeCommandListImmediateWithFlushTask(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies) {
    return executeCommandListImmediateWithFlushTaskImpl(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies, this->cmdQImmediate);
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::executeCommandListImmediateWithFlushTaskImpl(bool performMigration, bool hasStallingCmds, bool hasRelaxedOrderingDependencies, CommandQueue *cmdQ) {
    this->commandContainer.removeDuplicatesFromResidencyContainer();

    auto commandStream = this->commandContainer.getCommandStream();
    size_t commandStreamStart = this->cmdListCurrentStartOffset;

    auto csr = static_cast<CommandQueueImp *>(cmdQ)->getCsr();
    auto lockCSR = csr->obtainUniqueOwnership();

    std::unique_lock<std::mutex> lockForIndirect;
    if (this->hasIndirectAllocationsAllowed()) {
        cmdQ->handleIndirectAllocationResidency(this->getUnifiedMemoryControls(), lockForIndirect, performMigration);
    }

    if (performMigration) {
        auto deviceImp = static_cast<DeviceImp *>(this->device);
        auto pageFaultManager = deviceImp->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        if (pageFaultManager == nullptr) {
            performMigration = false;
        }
    }

    cmdQ->makeResidentAndMigrate(performMigration, this->commandContainer.getResidencyContainer());

    static_cast<CommandQueueHw<gfxCoreFamily> *>(this->cmdQImmediate)->patchCommands(*this, 0u);

    if (performMigration) {
        this->migrateSharedAllocations();
    }

    if (this->performMemoryPrefetch) {
        auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        prefetchManager->migrateAllocationsToGpu(this->getPrefetchContext(),
                                                 *this->device->getDriverHandle()->getSvmAllocsManager(),
                                                 *this->device->getNEODevice(),
                                                 *csr);
    }

    NEO::CompletionStamp completionStamp;
    if (isCopyOnly()) {
        completionStamp = flushBcsTask(*commandStream, commandStreamStart, hasStallingCmds, hasRelaxedOrderingDependencies, csr);
    } else {
        completionStamp = flushRegularTask(*commandStream, commandStreamStart, hasStallingCmds, hasRelaxedOrderingDependencies);
    }

    if (completionStamp.taskCount > NEO::CompletionStamp::notReady) {
        if (completionStamp.taskCount == NEO::CompletionStamp::outOfHostMemory) {
            return ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
        return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
    }

    if (this->isSyncModeQueue) {
        auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
        const auto waitStatus = csr->waitForCompletionWithTimeout(NEO::WaitParams{false, false, timeoutMicroseconds}, completionStamp.taskCount);
        if (waitStatus == NEO::WaitStatus::GpuHang) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
        csr->getInternalAllocationStorage()->cleanAllocationList(completionStamp.taskCount, NEO::AllocationUsage::TEMPORARY_ALLOCATION);
    }

    this->cmdListCurrentStartOffset = commandStream->getUsed();
    this->containsAnyKernel = false;

    this->handlePostSubmissionState();

    if (NEO::DebugManager.flags.PauseOnEnqueue.get() != -1) {
        this->device->getNEODevice()->debugExecutionCounter++;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::waitForEventsFromHost() {
    bool waitForEventsFromHostEnabled = this->isWaitForEventsFromHostEnabled();
    if (!waitForEventsFromHostEnabled) {
        return false;
    }

    auto numClients = static_cast<CommandQueueImp *>(this->cmdQImmediate)->getCsr()->getNumClients();
    auto numClientsLimit = 2u;
    if (NEO::DebugManager.flags.EventWaitOnHostNumClients.get() != -1) {
        numClientsLimit = NEO::DebugManager.flags.EventWaitOnHostNumClients.get();
    }
    if (numClients < numClientsLimit) {
        return false;
    };
    auto numThreadsLimit = 2u;
    if (NEO::DebugManager.flags.EventWaitOnHostNumThreads.get() != -1) {
        numThreadsLimit = NEO::DebugManager.flags.EventWaitOnHostNumThreads.get();
    }
    if (this->numThreads < numThreadsLimit) {
        return false;
    }

    return true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernel(
    ze_kernel_handle_t kernelHandle, const ze_group_count_t *threadGroupDimensions,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
    const CmdListKernelLaunchParams &launchParams) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    bool hostWait = waitForEventsFromHost();
    if (hostWait || this->eventWaitlistSyncRequired()) {
        this->synchronizeEventList(numWaitEvents, phWaitEvents);
        if (hostWait) {
            numWaitEvents = 0u;
            phWaitEvents = nullptr;
        }
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernel(kernelHandle, threadGroupDimensions,
                                                                        hSignalEvent, numWaitEvents, phWaitEvents,
                                                                        launchParams);
    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchKernelIndirect(
    ze_kernel_handle_t kernelHandle, const ze_group_count_t *pDispatchArgumentsBuffer,
    ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchKernelIndirect(kernelHandle, pDispatchArgumentsBuffer,
                                                                                hSignalEvent, numWaitEvents, phWaitEvents);
    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendBarrier(
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    ret = CommandListCoreFamily<gfxCoreFamily>::appendBarrier(hSignalEvent, numWaitEvents, phWaitEvents);

    this->dependenciesPresent = true;
    return flushImmediate(ret, true, true, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(
    void *dstptr,
    const void *srcptr,
    size_t size,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }

    ze_result_t ret;

    NEO::SvmAllocationData *srcAllocData = nullptr;
    NEO::SvmAllocationData *dstAllocData = nullptr;
    bool srcAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(const_cast<void *>(srcptr), size, &srcAllocData);
    bool dstAllocFound = this->device->getDriverHandle()->findAllocationDataForRange(dstptr, size, &dstAllocData);
    if (preferCopyThroughLockedPtr(dstAllocData, dstAllocFound, srcAllocData, srcAllocFound, size)) {
        return performCpuMemcpy(dstptr, srcptr, size, dstAllocData, srcAllocData, hSignalEvent, numWaitEvents, phWaitEvents);
    }

    if (this->isAppendSplitNeeded(dstptr, srcptr, size)) {
        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, void *, const void *>(this, dstptr, srcptr, size, hSignalEvent, true, (numWaitEvents > 0), [&](void *dstptrParam, const void *srcptrParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptrParam, srcptrParam, sizeParam, hSignalEventParam, numWaitEvents, phWaitEvents);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent,
                                                                     numWaitEvents, phWaitEvents);
    }
    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyRegion(
    void *dstPtr,
    const ze_copy_region_t *dstRegion,
    uint32_t dstPitch,
    uint32_t dstSlicePitch,
    const void *srcPtr,
    const ze_copy_region_t *srcRegion,
    uint32_t srcPitch,
    uint32_t srcSlicePitch,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }

    ze_result_t ret;

    if (this->isAppendSplitNeeded(dstPtr, srcPtr, this->getTotalSizeForCopyRegion(dstRegion, dstPitch, dstSlicePitch))) {
        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, uint32_t, uint32_t>(this, dstRegion->originX, srcRegion->originX, dstRegion->width, hSignalEvent, true, (numWaitEvents > 0), [&](uint32_t dstOriginXParam, uint32_t srcOriginXParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            ze_copy_region_t dstRegionLocal = {};
            ze_copy_region_t srcRegionLocal = {};
            memcpy(&dstRegionLocal, dstRegion, sizeof(ze_copy_region_t));
            memcpy(&srcRegionLocal, srcRegion, sizeof(ze_copy_region_t));
            dstRegionLocal.originX = dstOriginXParam;
            dstRegionLocal.width = static_cast<uint32_t>(sizeParam);
            srcRegionLocal.originX = srcOriginXParam;
            srcRegionLocal.width = static_cast<uint32_t>(sizeParam);
            return CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, &dstRegionLocal, dstPitch, dstSlicePitch,
                                                                                srcPtr, &srcRegionLocal, srcPitch, srcSlicePitch,
                                                                                hSignalEventParam, numWaitEvents, phWaitEvents);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryCopyRegion(dstPtr, dstRegion, dstPitch, dstSlicePitch,
                                                                           srcPtr, srcRegion, srcPitch, srcSlicePitch,
                                                                           hSignalEvent, numWaitEvents, phWaitEvents);
    }

    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryFill(void *ptr, const void *pattern,
                                                                            size_t patternSize, size_t size,
                                                                            ze_event_handle_t hSignalEvent,
                                                                            uint32_t numWaitEvents,
                                                                            ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryFill(ptr, pattern, patternSize, size, hSignalEvent, numWaitEvents, phWaitEvents);

    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendSignalEvent(ze_event_handle_t hSignalEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    ret = CommandListCoreFamily<gfxCoreFamily>::appendSignalEvent(hSignalEvent);
    return flushImmediate(ret, true, true, false, hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendEventReset(ze_event_handle_t hSignalEvent) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }
    ret = CommandListCoreFamily<gfxCoreFamily>::appendEventReset(hSignalEvent);
    return flushImmediate(ret, true, true, false, hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendPageFaultCopy(NEO::GraphicsAllocation *dstAllocation,
                                                                               NEO::GraphicsAllocation *srcAllocation,
                                                                               size_t size, bool flushHost) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
    }

    ze_result_t ret;

    if (this->isAppendSplitNeeded(dstAllocation->getMemoryPool(), srcAllocation->getMemoryPool(), size)) {
        uintptr_t dstAddress = static_cast<uintptr_t>(dstAllocation->getGpuAddress());
        uintptr_t srcAddress = static_cast<uintptr_t>(srcAllocation->getGpuAddress());
        ret = static_cast<DeviceImp *>(this->device)->bcsSplit.appendSplitCall<gfxCoreFamily, uintptr_t, uintptr_t>(this, dstAddress, srcAddress, size, nullptr, false, false, [&](uintptr_t dstAddressParam, uintptr_t srcAddressParam, size_t sizeParam, ze_event_handle_t hSignalEventParam) {
            this->appendMemoryCopyBlit(dstAddressParam, dstAllocation, 0u,
                                       srcAddressParam, srcAllocation, 0u,
                                       sizeParam);
            return this->appendSignalEvent(hSignalEventParam);
        });
    } else {
        ret = CommandListCoreFamily<gfxCoreFamily>::appendPageFaultCopy(dstAllocation, srcAllocation, size, flushHost);
    }
    return flushImmediate(ret, false, false, false, nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phWaitEvents, bool relaxedOrderingAllowed) {
    bool allSignaled = true;
    for (auto i = 0u; i < numEvents; i++) {
        allSignaled &= (!this->dcFlushSupport && Event::fromHandle(phWaitEvents[i])->isAlreadyCompleted());
    }
    if (allSignaled) {
        return ZE_RESULT_SUCCESS;
    }
    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWaitOnEvents(numEvents, phWaitEvents, relaxedOrderingAllowed);
    this->dependenciesPresent = true;
    return flushImmediate(ret, true, true, (numEvents > 0), nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendWriteGlobalTimestamp(
    uint64_t *dstptr, ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendWriteGlobalTimestamp(dstptr, hSignalEvent, numWaitEvents, phWaitEvents);

    return flushImmediate(ret, true, true, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopyFromContext(
    void *dstptr, ze_context_handle_t hContextSrc, const void *srcptr,
    size_t size, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryCopy(dstptr, srcptr, size, hSignalEvent, numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopy(
    ze_image_handle_t dst, ze_image_handle_t src,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    return CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(dst, src, nullptr, nullptr, hSignalEvent,
                                                                                numWaitEvents, phWaitEvents);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyRegion(ze_image_handle_t hDstImage,
                                                                                 ze_image_handle_t hSrcImage,
                                                                                 const ze_image_region_t *pDstRegion,
                                                                                 const ze_image_region_t *pSrcRegion,
                                                                                 ze_event_handle_t hSignalEvent,
                                                                                 uint32_t numWaitEvents,
                                                                                 ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyRegion(hDstImage, hSrcImage, pDstRegion, pSrcRegion, hSignalEvent,
                                                                           numWaitEvents, phWaitEvents);
    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyFromMemory(
    ze_image_handle_t hDstImage,
    const void *srcPtr,
    const ze_image_region_t *pDstRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyFromMemory(hDstImage, srcPtr, pDstRegion, hSignalEvent,
                                                                               numWaitEvents, phWaitEvents);

    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendImageCopyToMemory(
    void *dstPtr,
    ze_image_handle_t hSrcImage,
    const ze_image_region_t *pSrcRegion,
    ze_event_handle_t hSignalEvent,
    uint32_t numWaitEvents,
    ze_event_handle_t *phWaitEvents) {

    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendImageCopyToMemory(dstPtr, hSrcImage, pSrcRegion, hSignalEvent,
                                                                             numWaitEvents, phWaitEvents);

    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendMemoryRangesBarrier(uint32_t numRanges,
                                                                                     const size_t *pRangeSizes,
                                                                                     const void **pRanges,
                                                                                     ze_event_handle_t hSignalEvent,
                                                                                     uint32_t numWaitEvents,
                                                                                     ze_event_handle_t *phWaitEvents) {
    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, phWaitEvents);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendMemoryRangesBarrier(numRanges, pRangeSizes, pRanges, hSignalEvent, numWaitEvents, phWaitEvents);
    return flushImmediate(ret, true, true, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::appendLaunchCooperativeKernel(ze_kernel_handle_t kernelHandle,
                                                                                         const ze_group_count_t *launchKernelArgs,
                                                                                         ze_event_handle_t hSignalEvent,
                                                                                         uint32_t numWaitEvents,
                                                                                         ze_event_handle_t *waitEventHandles) {
    if (this->isFlushTaskSubmissionEnabled) {
        checkAvailableSpace();
        checkWaitEventsState(numWaitEvents, waitEventHandles);
    }
    auto ret = CommandListCoreFamily<gfxCoreFamily>::appendLaunchCooperativeKernel(kernelHandle, launchKernelArgs, hSignalEvent, numWaitEvents, waitEventHandles);
    return flushImmediate(ret, true, false, (numWaitEvents > 0), hSignalEvent);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::flushImmediate(ze_result_t inputRet, bool performMigration, bool hasStallingCmds,
                                                                          bool hasRelaxedOrderingDependencies, ze_event_handle_t hSignalEvent) {
    if (inputRet == ZE_RESULT_SUCCESS) {
        if (this->isFlushTaskSubmissionEnabled) {
            inputRet = executeCommandListImmediateWithFlushTask(performMigration, hasStallingCmds, hasRelaxedOrderingDependencies);
        } else {
            inputRet = executeCommandListImmediate(performMigration);
        }
    }
    if (hSignalEvent) {
        Event::fromHandle(hSignalEvent)->setCsr(this->csr);
    }
    return inputRet;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::preferCopyThroughLockedPtr(NEO::SvmAllocationData *dstAlloc, bool dstFound, NEO::SvmAllocationData *srcAlloc, bool srcFound, size_t size) {
    if (NEO::DebugManager.flags.ExperimentalForceCopyThroughLock.get() == 1) {
        return true;
    }

    size_t h2DThreshold = 2 * MemoryConstants::megaByte;
    size_t d2HThreshold = 1 * MemoryConstants::kiloByte;
    if (NEO::DebugManager.flags.ExperimentalH2DCpuCopyThreshold.get() != -1) {
        h2DThreshold = NEO::DebugManager.flags.ExperimentalH2DCpuCopyThreshold.get();
    }
    if (NEO::DebugManager.flags.ExperimentalD2HCpuCopyThreshold.get() != -1) {
        d2HThreshold = NEO::DebugManager.flags.ExperimentalD2HCpuCopyThreshold.get();
    }

    auto &gfxCoreHelper = this->device->getGfxCoreHelper();
    if (gfxCoreHelper.copyThroughLockedPtrEnabled(this->device->getHwInfo())) {
        return (!srcFound && isSuitableUSMDeviceAlloc(dstAlloc, dstFound) && size <= h2DThreshold) ||
               (!dstFound && isSuitableUSMDeviceAlloc(srcAlloc, srcFound) && size <= d2HThreshold);
    }
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::isSuitableUSMDeviceAlloc(NEO::SvmAllocationData *alloc, bool allocFound) {
    return allocFound && (alloc->memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY) &&
           alloc->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex())->storageInfo.getNumBanks() == 1;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamilyImmediate<gfxCoreFamily>::performCpuMemcpy(void *dstptr, const void *srcptr, size_t size, NEO::SvmAllocationData *dstAlloc, NEO::SvmAllocationData *srcAlloc, ze_event_handle_t hSignalEvent, uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {

    bool needsBarrier = (numWaitEvents > 0);
    if (needsBarrier) {
        this->appendBarrier(nullptr, numWaitEvents, phWaitEvents);
    }

    if (this->dependenciesPresent) {
        auto submissionStatus = this->csr->flushTagUpdate();
        if (submissionStatus != NEO::SubmissionStatus::SUCCESS) {
            return getErrorCodeForSubmissionStatus(submissionStatus);
        }
    }

    Event *signalEvent = nullptr;
    if (hSignalEvent) {
        signalEvent = Event::fromHandle(hSignalEvent);
    }

    auto srcLockPointer = obtainLockedPtrFromDevice(srcAlloc, const_cast<void *>(srcptr));
    auto dstLockPointer = obtainLockedPtrFromDevice(dstAlloc, dstptr);

    const void *cpuMemcpySrcPtr = srcLockPointer ? srcLockPointer : srcptr;
    void *cpuMemcpyDstPtr = dstLockPointer ? dstLockPointer : dstptr;

    if (this->dependenciesPresent) {
        auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
        const auto waitStatus = this->csr->waitForCompletionWithTimeout(NEO::WaitParams{false, false, timeoutMicroseconds}, this->csr->peekTaskCount());
        if (waitStatus == NEO::WaitStatus::GpuHang) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
        this->dependenciesPresent = false;
    }

    if (signalEvent) {
        signalEvent->setGpuStartTimestamp();
    }

    memcpy_s(cpuMemcpyDstPtr, size, cpuMemcpySrcPtr, size);

    if (signalEvent) {
        signalEvent->setGpuEndTimestamp();
        signalEvent->hostSignal();
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void *CommandListCoreFamilyImmediate<gfxCoreFamily>::obtainLockedPtrFromDevice(NEO::SvmAllocationData *allocData, void *ptr) {
    if (!allocData) {
        return nullptr;
    }

    auto alloc = allocData->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex());
    if (alloc->getMemoryPool() != NEO::MemoryPool::LocalMemory) {
        return nullptr;
    }

    if (!alloc->isLocked()) {
        this->device->getDriverHandle()->getMemoryManager()->lockResource(alloc);
    }
    auto gpuAddress = allocData->gpuAllocations.getGraphicsAllocation(this->device->getRootDeviceIndex())->getGpuAddress();
    auto offset = ptrDiff(ptr, gpuAddress);
    return ptrOffset(alloc->getLockedPtr(), offset);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamilyImmediate<gfxCoreFamily>::checkWaitEventsState(uint32_t numWaitEvents, ze_event_handle_t *waitEventList) {
    if (this->eventWaitlistSyncRequired()) {
        this->synchronizeEventList(numWaitEvents, waitEventList);
    }
}

} // namespace L0
