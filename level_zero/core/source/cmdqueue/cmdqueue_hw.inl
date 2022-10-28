/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/tools/source/metrics/metric.h"

#include <algorithm>
#include <limits>
#include <thread>

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::createFence(const ze_fence_desc_t *desc,
                                                       ze_fence_handle_t *phFence) {
    *phFence = Fence::create(this, desc);
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::destroy() {
    if (commandStream.getCpuBase() != nullptr) {
        commandStream.replaceGraphicsAllocation(nullptr);
        commandStream.replaceBuffer(nullptr, 0);
    }
    buffers.destroy(this->getDevice());
    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
        device->getL0Debugger()->notifyCommandQueueDestroyed(device->getNEODevice());
    }
    delete this;
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandLists(
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence,
    bool performMigration) {

    auto lockCSR = this->csr->obtainUniqueOwnership();

    auto ctx = CommandListExecutionContext{phCommandLists,
                                           numCommandLists,
                                           csr->getPreemptionMode(),
                                           device,
                                           NEO::Debugger::isDebugEnabled(internalUsage),
                                           csr->isProgramActivePartitionConfigRequired(),
                                           performMigration};

    auto ret = validateCommandListsParams(ctx, phCommandLists, numCommandLists);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    this->device->activateMetricGroups();

    if (this->isCopyOnlyCommandQueue) {
        ret = this->executeCommandListsCopyOnly(ctx, numCommandLists, phCommandLists, hFence);
    } else {
        ret = this->executeCommandListsRegular(ctx, numCommandLists, phCommandLists, hFence);
    }

    if (NEO::DebugManager.flags.PauseOnEnqueue.get() != -1) {
        this->device->getNEODevice()->debugExecutionCounter++;
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandListsRegular(
    CommandListExecutionContext &ctx,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence) {

    this->setupCmdListsAndContextParams(ctx, phCommandLists, numCommandLists, hFence);
    ctx.isDirectSubmissionEnabled = this->csr->isDirectSubmissionEnabled();

    std::unique_lock<std::mutex> lockForIndirect;
    if (ctx.hasIndirectAccess) {
        handleIndirectAllocationResidency(ctx.unifiedMemoryControls, lockForIndirect);
    }

    size_t linearStreamSizeEstimate = this->estimateLinearStreamSizeInitial(ctx, phCommandLists, numCommandLists);

    this->handleScratchSpaceAndUpdateGSBAStateDirtyFlag(ctx);
    this->setFrontEndStateProperties(ctx);

    linearStreamSizeEstimate += this->estimateLinearStreamSizeComplementary(ctx, phCommandLists, numCommandLists);
    linearStreamSizeEstimate += this->computePreemptionSize(ctx, phCommandLists, numCommandLists);
    linearStreamSizeEstimate += this->computeDebuggerCmdsSize(ctx);

    if (ctx.isDispatchTaskCountPostSyncRequired) {
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(this->device->getHwInfo(), false);
    }

    NEO::LinearStream child(nullptr);
    if (const auto ret = this->makeAlignedChildStreamAndSetGpuBase(child, linearStreamSizeEstimate); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    this->allocateGlobalFenceAndMakeItResident();
    this->allocateWorkPartitionAndMakeItResident();
    this->allocateTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(child);
    this->csr->programHardwareContext(child);
    this->makeSbaTrackingBufferResidentIfL0DebuggerEnabled(ctx.isDebugEnabled);

    auto &csrStateProperties = csr->getStreamProperties();
    if (!this->pipelineSelectStateTracking) {
        this->programPipelineSelectIfGpgpuDisabled(child);
    } else {
        // Setting systolic/pipeline select here for 1st command list is to preserve dispatch order of hw commands
        auto commandList = CommandList::fromHandle(phCommandLists[0]);
        auto &requiredStreamState = commandList->getRequiredStreamState();
        // Provide cmdlist required state as cmdlist final state, so csr state does not transition to final
        // By preserving required state in csr - keeping csr state not dirty - it will not dispatch 1st command list pipeline select/systolic in main loop
        // Csr state will transition to final of 1st command list in main loop
        this->programOneCmdListPipelineSelect(commandList, child, csrStateProperties, requiredStreamState, requiredStreamState);
    }
    this->programCommandQueueDebugCmdsForSourceLevelOrL0DebuggerIfEnabled(ctx.isDebugEnabled, child);
    this->programStateBaseAddressWithGsbaIfDirty(ctx, phCommandLists[0], child);
    this->programCsrBaseAddressIfPreemptionModeInitial(ctx.isPreemptionModeInitial, child);
    this->programStateSip(ctx.stateSipRequired, child);
    this->makePreemptionAllocationResidentForModeMidThread(ctx.isDevicePreemptionModeMidThread);
    this->makeSipIsaResidentIfSipKernelUsed(ctx);
    this->makeDebugSurfaceResidentIfNEODebuggerActive(ctx.isNEODebuggerActive(this->device));

    this->programActivePartitionConfig(ctx.isProgramActivePartitionConfigRequired, child);
    this->encodeKernelArgsBufferAndMakeItResident();

    bool shouldProgramVfe = this->csr->getLogicalStateHelper() && ctx.frontEndStateDirty;
    this->programFrontEndAndClearDirtyFlag(shouldProgramVfe, ctx, child, csrStateProperties);

    this->writeCsrStreamInlineIfLogicalStateHelperAvailable(child);

    ctx.statePreemption = ctx.preemptionMode;

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto &requiredStreamState = commandList->getRequiredStreamState();
        auto &finalStreamState = commandList->getFinalStreamState();

        this->updateOneCmdListPreemptionModeAndCtxStatePreemption(ctx, commandList->getCommandListPreemptionMode(), child);

        this->programOneCmdListPipelineSelect(commandList, child, csrStateProperties, requiredStreamState, finalStreamState);
        this->programOneCmdListFrontEndIfDirty(ctx, child, csrStateProperties, requiredStreamState, finalStreamState);
        this->programRequiredStateComputeModeForCommandList(commandList, child, csrStateProperties, requiredStreamState, finalStreamState);

        this->patchCommands(*commandList, this->csr->getScratchSpaceController()->getScratchPatchAddress());
        this->programOneCmdListBatchBufferStart(commandList, child, ctx);
        this->mergeOneCmdListPipelinedState(commandList);
    }

    this->updateBaseAddressState(CommandList::fromHandle(phCommandLists[numCommandLists - 1]));
    this->collectPrintfContentsFromAllCommandsLists(phCommandLists, numCommandLists);
    this->migrateSharedAllocationsIfRequested(ctx.isMigrationRequested, phCommandLists[0]);
    this->prefetchMemoryIfRequested(ctx.performMemoryPrefetch);

    this->programStateSipEndWA(ctx.stateSipRequired, child);

    this->csr->setPreemptionMode(ctx.statePreemption);
    this->assignCsrTaskCountToFenceIfAvailable(hFence);

    this->dispatchTaskCountPostSyncRegular(ctx.isDispatchTaskCountPostSyncRequired, child);

    this->makeCsrTagAllocationResident();
    auto submitResult = this->prepareAndSubmitBatchBuffer(ctx, child);
    this->updateTaskCountAndPostSync(ctx.isDispatchTaskCountPostSyncRequired);
    this->csr->makeSurfacePackNonResident(this->csr->getResidencyAllocations(), false);

    auto completionResult = this->waitForCommandQueueCompletionAndCleanHeapContainer();
    ze_result_t retVal = this->handleSubmissionAndCompletionResults(submitResult, completionResult);

    this->csr->getResidencyAllocations().clear();

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandListsCopyOnly(
    CommandListExecutionContext &ctx,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence) {

    this->setupCmdListsAndContextParams(ctx, phCommandLists, numCommandLists, hFence);
    ctx.isDirectSubmissionEnabled = this->csr->isBlitterDirectSubmissionEnabled();

    size_t linearStreamSizeEstimate = this->estimateLinearStreamSizeInitial(ctx, phCommandLists, numCommandLists);

    this->csr->getResidencyAllocations().reserve(ctx.spaceForResidency);

    linearStreamSizeEstimate += NEO::EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();

    NEO::LinearStream child(nullptr);
    if (const auto ret = this->makeAlignedChildStreamAndSetGpuBase(child, linearStreamSizeEstimate); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    this->allocateGlobalFenceAndMakeItResident();
    this->allocateTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(child);
    this->csr->programHardwareContext(child);

    this->encodeKernelArgsBufferAndMakeItResident();

    this->writeCsrStreamInlineIfLogicalStateHelperAvailable(child);

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        this->programOneCmdListBatchBufferStart(commandList, child);
        this->mergeOneCmdListPipelinedState(commandList);
    }
    this->migrateSharedAllocationsIfRequested(ctx.isMigrationRequested, phCommandLists[0]);

    this->assignCsrTaskCountToFenceIfAvailable(hFence);

    this->dispatchTaskCountPostSyncByMiFlushDw(ctx.isDispatchTaskCountPostSyncRequired, child);

    this->makeCsrTagAllocationResident();
    auto submitResult = this->prepareAndSubmitBatchBuffer(ctx, child);
    this->updateTaskCountAndPostSync(ctx.isDispatchTaskCountPostSyncRequired);
    this->csr->makeSurfacePackNonResident(this->csr->getResidencyAllocations(), false);

    auto completionResult = this->waitForCommandQueueCompletionAndCleanHeapContainer();
    ze_result_t retVal = this->handleSubmissionAndCompletionResults(submitResult, completionResult);

    this->csr->getResidencyAllocations().clear();

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::validateCommandListsParams(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists) {

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        if (this->peekIsCopyOnlyCommandQueue() != commandList->isCopyOnly()) {
            return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
        }

        if (this->activeSubDevices < commandList->partitionCount) {
            return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
        }
    }

    if (ctx.anyCommandListWithCooperativeKernels &&
        ctx.anyCommandListWithoutCooperativeKernels &&
        (!NEO::DebugManager.flags.AllowMixingRegularAndCooperativeKernels.get())) {
        return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListFrontEndIfDirty(
    CommandListExecutionContext &ctx,
    NEO::LinearStream &cmdStream,
    NEO::StreamProperties &csrState,
    const NEO::StreamProperties &cmdListRequired,
    const NEO::StreamProperties &cmdListFinal) {

    bool shouldProgramVfe = ctx.frontEndStateDirty;

    ctx.cmdListBeginState.frontEndState = {};

    if (frontEndTrackingEnabled()) {
        csrState.frontEndState.setProperties(cmdListRequired.frontEndState);
        csrState.frontEndState.setPropertySingleSliceDispatchCcsMode(ctx.engineInstanced, device->getHwInfo());

        shouldProgramVfe |= csrState.frontEndState.isDirty();
    }

    ctx.cmdListBeginState.frontEndState.setProperties(csrState.frontEndState);
    this->programFrontEndAndClearDirtyFlag(shouldProgramVfe, ctx, cmdStream, csrState);

    if (frontEndTrackingEnabled()) {
        csrState.frontEndState.setProperties(cmdListFinal.frontEndState);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programFrontEndAndClearDirtyFlag(
    bool shouldFrontEndBeProgrammed,
    CommandListExecutionContext &ctx,
    NEO::LinearStream &cmdStream,
    NEO::StreamProperties &csrState) {

    if (!shouldFrontEndBeProgrammed) {
        return;
    }
    auto scratchSpaceController = this->csr->getScratchSpaceController();
    programFrontEnd(scratchSpaceController->getScratchPatchAddress(),
                    scratchSpaceController->getPerThreadScratchSpaceSize(),
                    cmdStream,
                    csrState);
    ctx.frontEndStateDirty = false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &cmdStream, NEO::StreamProperties &streamProperties) {
    UNRECOVERABLE_IF(csr == nullptr);
    auto &hwInfo = device->getHwInfo();
    auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(csr->getOsContext().getEngineType(),
                                                       csr->getOsContext().getEngineUsage(), hwInfo);
    auto pVfeState = NEO::PreambleHelper<GfxFamily>::getSpaceForVfeState(&cmdStream, hwInfo, engineGroupType);
    NEO::PreambleHelper<GfxFamily>::programVfeState(pVfeState,
                                                    hwInfo,
                                                    perThreadScratchSpaceSize,
                                                    scratchAddress,
                                                    device->getMaxNumHwThreads(),
                                                    streamProperties,
                                                    csr->getLogicalStateHelper());
    csr->setMediaVFEStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSize() {
    return NEO::PreambleHelper<GfxFamily>::getVFECommandsSize();
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSize(bool isFrontEndDirty) {
    if (!frontEndTrackingEnabled()) {
        return isFrontEndDirty * estimateFrontEndCmdSize();
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSizeForMultipleCommandLists(
    bool &isFrontEndStateDirty, int32_t engineInstanced, CommandList *commandList,
    NEO::StreamProperties &csrStateCopy,
    const NEO::StreamProperties &cmdListRequired,
    const NEO::StreamProperties &cmdListFinal) {

    if (!frontEndTrackingEnabled()) {
        return 0;
    }

    auto singleFrontEndCmdSize = estimateFrontEndCmdSize();
    size_t estimatedSize = 0;

    csrStateCopy.frontEndState.setProperties(cmdListRequired.frontEndState);
    csrStateCopy.frontEndState.setPropertySingleSliceDispatchCcsMode(engineInstanced, device->getHwInfo());
    if (isFrontEndStateDirty || csrStateCopy.frontEndState.isDirty()) {
        estimatedSize += singleFrontEndCmdSize;
        isFrontEndStateDirty = false;
    }
    if (this->frontEndStateTracking) {
        uint32_t frontEndChanges = commandList->getReturnPointsSize();
        estimatedSize += (frontEndChanges * singleFrontEndCmdSize);
        estimatedSize += (frontEndChanges * NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize());
    }
    csrStateCopy.frontEndState.setProperties(cmdListFinal.frontEndState);

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programPipelineSelectIfGpgpuDisabled(NEO::LinearStream &cmdStream) {
    bool gpgpuEnabled = this->csr->getPreambleSetFlag();
    if (!gpgpuEnabled) {
        NEO::PipelineSelectArgs args = {false, false, false, false};
        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(&cmdStream, args, device->getHwInfo());
        this->csr->setPreambleSetFlag(true);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence, bool containsAnyRegularCmdList) const {
    return containsAnyRegularCmdList || !csr->isUpdateTagFromWaitEnabled() || hFence != nullptr || getSynchronousMode() == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::getPreemptionCmdProgramming() {
    return NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(NEO::PreemptionMode::MidThread, NEO::PreemptionMode::Initial) > 0u;
}

template <GFXCORE_FAMILY gfxCoreFamily>
CommandQueueHw<gfxCoreFamily>::CommandListExecutionContext::CommandListExecutionContext(
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists,
    NEO::PreemptionMode contextPreemptionMode,
    Device *device,
    bool debugEnabled,
    bool programActivePartitionConfig,
    bool performMigration) : preemptionMode{contextPreemptionMode},
                             statePreemption{contextPreemptionMode},
                             isPreemptionModeInitial{contextPreemptionMode == NEO::PreemptionMode::Initial},
                             isDebugEnabled{debugEnabled},
                             isProgramActivePartitionConfigRequired{programActivePartitionConfig},
                             isMigrationRequested{performMigration} {

    constexpr size_t residencyContainerSpaceForPreemption = 2;
    constexpr size_t residencyContainerSpaceForTagWrite = 1;

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);

        if (commandList->containsCooperativeKernels()) {
            this->anyCommandListWithCooperativeKernels = true;
        } else {
            this->anyCommandListWithoutCooperativeKernels = true;
        }

        if (commandList->getRequiredStreamState().frontEndState.disableEUFusion.value == 1) {
            this->anyCommandListRequiresDisabledEUFusion = true;
        }

        // If the Command List has commands that require uncached MOCS, then any changes to the commands in the queue requires the uncached MOCS
        if (commandList->requiresQueueUncachedMocs && this->cachedMOCSAllowed == true) {
            this->cachedMOCSAllowed = false;
        }

        if (commandList->isMemoryPrefetchRequested()) {
            this->performMemoryPrefetch = true;
        }
        hasIndirectAccess |= commandList->hasIndirectAllocationsAllowed();
        if (commandList->hasIndirectAllocationsAllowed()) {
            unifiedMemoryControls.indirectDeviceAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed;
            unifiedMemoryControls.indirectHostAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectHostAllocationsAllowed;
            unifiedMemoryControls.indirectSharedAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectSharedAllocationsAllowed;
        }
    }
    this->isDevicePreemptionModeMidThread = device->getDevicePreemptionMode() == NEO::PreemptionMode::MidThread;
    this->stateSipRequired = (this->isPreemptionModeInitial && this->isDevicePreemptionModeMidThread) ||
                             this->isNEODebuggerActive(device);

    if (this->isDevicePreemptionModeMidThread) {
        this->spaceForResidency += residencyContainerSpaceForPreemption;
    }
    this->spaceForResidency += residencyContainerSpaceForTagWrite;

    if (this->isMigrationRequested && device->getDriverHandle()->getMemoryManager()->getPageFaultManager() == nullptr) {
        this->isMigrationRequested = false;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::CommandListExecutionContext::isNEODebuggerActive(Device *device) {
    return device->getNEODevice()->getDebugger() && this->isDebugEnabled;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::computeDebuggerCmdsSize(const CommandListExecutionContext &ctx) {
    size_t debuggerCmdsSize = 0;

    if (ctx.isDebugEnabled && !this->commandQueueDebugCmdsProgrammed) {
        if (this->device->getNEODevice()->getSourceLevelDebugger()) {
            debuggerCmdsSize += NEO::PreambleHelper<GfxFamily>::getKernelDebuggingCommandsSize(true);
        } else if (this->device->getL0Debugger()) {
            debuggerCmdsSize += device->getL0Debugger()->getSbaAddressLoadCommandsSize();
        }
    }

    return debuggerCmdsSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::computePreemptionSize(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists) {

    size_t preemptionSize = 0u;
    NEO::Device *neoDevice = this->device->getNEODevice();

    if (ctx.isPreemptionModeInitial) {
        preemptionSize += NEO::PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*neoDevice);
    }

    if (ctx.stateSipRequired) {
        preemptionSize += NEO::PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*neoDevice, this->csr->isRcs());
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto commandListPreemption = commandList->getCommandListPreemptionMode();

        if (ctx.statePreemption != commandListPreemption) {
            if (this->preemptionCmdSyncProgramming) {
                preemptionSize += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
            }
            preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(commandListPreemption, ctx.statePreemption);
            ctx.statePreemption = commandListPreemption;
        }
    }

    return preemptionSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::setupCmdListsAndContextParams(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists,
    ze_fence_handle_t hFence) {

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);

        commandList->csr = this->csr;

        ctx.containsAnyRegularCmdList |= commandList->cmdListType == CommandList::CommandListType::TYPE_REGULAR;
        if (!isCopyOnlyCommandQueue) {
            ctx.perThreadScratchSpaceSize = std::max(ctx.perThreadScratchSpaceSize, commandList->getCommandListPerThreadScratchSize());
            ctx.perThreadPrivateScratchSize = std::max(ctx.perThreadPrivateScratchSize, commandList->getCommandListPerThreadPrivateScratchSize());

            if (commandList->getCommandListPerThreadScratchSize() != 0 || commandList->getCommandListPerThreadPrivateScratchSize() != 0) {
                if (commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE) != nullptr) {
                    heapContainer.push_back(commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE)->getGraphicsAllocation());
                }
                for (auto element : commandList->commandContainer.sshAllocations) {
                    heapContainer.push_back(element);
                }
            }
        }

        this->partitionCount = std::max(this->partitionCount, commandList->partitionCount);
        commandList->makeResidentAndMigrate(ctx.isMigrationRequested);
    }

    ctx.isDispatchTaskCountPostSyncRequired = isDispatchTaskCountPostSyncRequired(hFence, ctx.containsAnyRegularCmdList);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeInitial(
    const CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists) {

    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    size_t linearStreamSizeEstimate = 0u;

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        linearStreamSizeEstimate += commandList->commandContainer.getCmdBufferAllocations().size();
    }
    linearStreamSizeEstimate *= NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
    linearStreamSizeEstimate += this->csr->getCmdsSizeForHardwareContext();

    if (ctx.isDirectSubmissionEnabled) {
        linearStreamSizeEstimate += NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
    } else {
        linearStreamSizeEstimate += NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferEndSize();
    }

    auto csrHw = reinterpret_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
    if (ctx.isProgramActivePartitionConfigRequired) {
        linearStreamSizeEstimate += csrHw->getCmdSizeForActivePartitionConfig();
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        linearStreamSizeEstimate += NEO::SWTagsManager::estimateSpaceForSWTags<GfxFamily>();
    }

    linearStreamSizeEstimate += NEO::EncodeKernelArgsBuffer<GfxFamily>::getKernelArgsBufferCmdsSize(this->csr->getKernelArgsBufferAllocation(),
                                                                                                    this->csr->getLogicalStateHelper());

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::DebugManager.flags.PauseOnEnqueue.get(),
                                                    this->device->getNEODevice()->debugExecutionCounter.load(),
                                                    NEO::PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(this->device->getHwInfo(), false);
        linearStreamSizeEstimate += sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
    }

    if (NEO::PauseOnGpuProperties::pauseModeAllowed(NEO::DebugManager.flags.PauseOnEnqueue.get(),
                                                    this->device->getNEODevice()->debugExecutionCounter.load(),
                                                    NEO::PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(this->device->getHwInfo(), false);
        linearStreamSizeEstimate += sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
    }

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::setFrontEndStateProperties(CommandListExecutionContext &ctx) {
    const auto &hwInfo = this->device->getHwInfo();

    auto isEngineInstanced = csr->getOsContext().isEngineInstanced();
    auto &streamProperties = this->csr->getStreamProperties();
    if (!frontEndTrackingEnabled()) {
        streamProperties.frontEndState.setProperties(ctx.anyCommandListWithCooperativeKernels, ctx.anyCommandListRequiresDisabledEUFusion,
                                                     true, isEngineInstanced, hwInfo);
        ctx.frontEndStateDirty |= (streamProperties.frontEndState.isDirty() && !this->csr->getLogicalStateHelper());
    } else {
        ctx.engineInstanced = isEngineInstanced;
    }
    ctx.frontEndStateDirty |= csr->getMediaVFEStateDirty();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpaceAndUpdateGSBAStateDirtyFlag(CommandListExecutionContext &ctx) {
    handleScratchSpace(this->heapContainer,
                       this->csr->getScratchSpaceController(),
                       ctx.gsbaStateDirty, ctx.frontEndStateDirty,
                       ctx.perThreadScratchSpaceSize, ctx.perThreadPrivateScratchSize);
    ctx.gsbaStateDirty |= this->csr->getGSBAStateDirty();
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeComplementary(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists) {

    size_t linearStreamSizeEstimate = 0u;

    linearStreamSizeEstimate += estimateFrontEndCmdSize(ctx.frontEndStateDirty);
    linearStreamSizeEstimate += estimatePipelineSelectCmdSize();

    if (this->stateComputeModeTracking || this->pipelineSelectStateTracking || frontEndTrackingEnabled()) {
        bool frontEndStateDirtyCopy = ctx.frontEndStateDirty;
        auto streamPropertiesCopy = csr->getStreamProperties();
        bool gpgpuEnabledCopy = csr->getPreambleSetFlag();
        for (uint32_t i = 0; i < numCommandLists; i++) {
            auto cmdList = CommandList::fromHandle(phCommandLists[i]);
            auto &requiredStreamState = cmdList->getRequiredStreamState();
            auto &finalStreamState = cmdList->getFinalStreamState();

            linearStreamSizeEstimate += estimateFrontEndCmdSizeForMultipleCommandLists(frontEndStateDirtyCopy, ctx.engineInstanced, cmdList,
                                                                                       streamPropertiesCopy, requiredStreamState, finalStreamState);
            linearStreamSizeEstimate += estimatePipelineSelectCmdSizeForMultipleCommandLists(streamPropertiesCopy, requiredStreamState, finalStreamState, gpgpuEnabledCopy);
            linearStreamSizeEstimate += estimateScmCmdSizeForMultipleCommandLists(streamPropertiesCopy, requiredStreamState, finalStreamState);
        }
    }

    if (ctx.gsbaStateDirty) {
        linearStreamSizeEstimate += estimateStateBaseAddressCmdSize();
    }

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::makeAlignedChildStreamAndSetGpuBase(NEO::LinearStream &child, size_t requiredSize) {

    size_t alignedSize = alignUp<size_t>(requiredSize, this->minCmdBufferPtrAlign);

    if (const auto waitStatus = this->reserveLinearStreamSize(alignedSize); waitStatus == NEO::WaitStatus::GpuHang) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    child.replaceBuffer(this->commandStream.getSpace(alignedSize), alignedSize);
    child.setGpuBase(ptrOffset(this->commandStream.getGpuBase(), this->commandStream.getUsed() - alignedSize));
    this->alignedChildStreamPadding = alignedSize - requiredSize;
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::allocateGlobalFenceAndMakeItResident() {
    const auto globalFenceAllocation = this->csr->getGlobalFenceAllocation();
    if (globalFenceAllocation) {
        this->csr->makeResident(*globalFenceAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::allocateWorkPartitionAndMakeItResident() {
    const auto workPartitionAllocation = this->csr->getWorkPartitionAllocation();
    if (workPartitionAllocation) {
        this->csr->makeResident(*workPartitionAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::allocateTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(NEO::LinearStream &cmdStream) {
    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        NEO::Device *neoDevice = this->device->getNEODevice();
        NEO::SWTagsManager *tagsManager = neoDevice->getRootDeviceEnvironment().tagsManager.get();
        UNRECOVERABLE_IF(tagsManager == nullptr);
        this->csr->makeResident(*tagsManager->getBXMLHeapAllocation());
        this->csr->makeResident(*tagsManager->getSWTagHeapAllocation());
        tagsManager->insertBXMLHeapAddress<GfxFamily>(cmdStream);
        tagsManager->insertSWTagHeapAddress<GfxFamily>(cmdStream);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeSbaTrackingBufferResidentIfL0DebuggerEnabled(bool isDebugEnabled) {
    if (isDebugEnabled && this->device->getL0Debugger()) {
        this->csr->makeResident(*this->device->getL0Debugger()->getSbaTrackingBuffer(this->csr->getOsContext().getContextId()));
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programCommandQueueDebugCmdsForSourceLevelOrL0DebuggerIfEnabled(bool isDebugEnabled, NEO::LinearStream &cmdStream) {
    if (isDebugEnabled && !this->commandQueueDebugCmdsProgrammed) {
        NEO::Device *neoDevice = device->getNEODevice();
        if (neoDevice->getSourceLevelDebugger()) {
            NEO::PreambleHelper<GfxFamily>::programKernelDebugging(&cmdStream);
            this->commandQueueDebugCmdsProgrammed = true;
        } else if (this->device->getL0Debugger()) {
            this->device->getL0Debugger()->programSbaAddressLoad(cmdStream,
                                                                 device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress());
            this->commandQueueDebugCmdsProgrammed = true;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateBaseAddressWithGsbaIfDirty(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t hCommandList,
    NEO::LinearStream &cmdStream) {

    if (!ctx.gsbaStateDirty) {
        return;
    }
    auto indirectHeap = CommandList::fromHandle(hCommandList)->commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
    auto scratchSpaceController = this->csr->getScratchSpaceController();
    programStateBaseAddress(scratchSpaceController->calculateNewGSH(),
                            indirectHeap->getGraphicsAllocation()->isAllocatedInLocalMemoryPool(),
                            cmdStream,
                            ctx.cachedMOCSAllowed);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programCsrBaseAddressIfPreemptionModeInitial(bool isPreemptionModeInitial, NEO::LinearStream &cmdStream) {
    if (!isPreemptionModeInitial) {
        return;
    }
    NEO::Device *neoDevice = this->device->getNEODevice();
    NEO::PreemptionHelper::programCsrBaseAddress<GfxFamily>(cmdStream,
                                                            *neoDevice,
                                                            this->csr->getPreemptionAllocation(),
                                                            this->csr->getLogicalStateHelper());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateSip(bool isStateSipRequired, NEO::LinearStream &cmdStream) {
    if (!isStateSipRequired) {
        return;
    }
    NEO::Device *neoDevice = this->device->getNEODevice();
    NEO::PreemptionHelper::programStateSip<GfxFamily>(cmdStream, *neoDevice, this->csr->getLogicalStateHelper());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateSipEndWA(bool isStateSipRequired, NEO::LinearStream &cmdStream) {
    if (!isStateSipRequired) {
        return;
    }
    NEO::Device *neoDevice = this->device->getNEODevice();
    NEO::PreemptionHelper::programStateSipEndWa<GfxFamily>(cmdStream, *neoDevice);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateOneCmdListPreemptionModeAndCtxStatePreemption(
    CommandListExecutionContext &ctx,
    NEO::PreemptionMode commandListPreemption,
    NEO::LinearStream &cmdStream) {

    NEO::Device *neoDevice = this->device->getNEODevice();
    if (ctx.statePreemption != commandListPreemption) {
        if (NEO::DebugManager.flags.EnableSWTags.get()) {
            neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::PipeControlReasonTag>(
                cmdStream,
                *neoDevice,
                "ComandList Preemption Mode update", 0u);
        }

        if (this->preemptionCmdSyncProgramming) {
            NEO::PipeControlArgs args;
            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);
        }
        NEO::PreemptionHelper::programCmdStream<GfxFamily>(cmdStream,
                                                           commandListPreemption,
                                                           ctx.statePreemption,
                                                           this->csr->getPreemptionAllocation());
        ctx.statePreemption = commandListPreemption;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makePreemptionAllocationResidentForModeMidThread(bool isDevicePreemptionModeMidThread) {
    if (isDevicePreemptionModeMidThread) {
        this->csr->makeResident(*this->csr->getPreemptionAllocation());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeSipIsaResidentIfSipKernelUsed(CommandListExecutionContext &ctx) {
    NEO::Device *neoDevice = this->device->getNEODevice();
    if (ctx.isDevicePreemptionModeMidThread || ctx.isNEODebuggerActive(this->device)) {
        auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
        this->csr->makeResident(*sipIsa);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeDebugSurfaceResidentIfNEODebuggerActive(bool isNEODebuggerActive) {
    if (!isNEODebuggerActive) {
        return;
    }
    UNRECOVERABLE_IF(this->device->getDebugSurface() == nullptr);
    this->csr->makeResident(*this->device->getDebugSurface());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programActivePartitionConfig(
    bool isProgramActivePartitionConfigRequired,
    NEO::LinearStream &cmdStream) {

    if (!isProgramActivePartitionConfigRequired) {
        return;
    }
    auto csrHw = reinterpret_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
    csrHw->programActivePartitionConfig(cmdStream);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::encodeKernelArgsBufferAndMakeItResident() {
    NEO::EncodeKernelArgsBuffer<GfxFamily>::encodeKernelArgsBufferCmds(this->csr->getKernelArgsBufferAllocation(),
                                                                       this->csr->getLogicalStateHelper());
    if (this->csr->getKernelArgsBufferAllocation()) {
        this->csr->makeResident(*this->csr->getKernelArgsBufferAllocation());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::writeCsrStreamInlineIfLogicalStateHelperAvailable(NEO::LinearStream &cmdStream) {
    if (this->csr->getLogicalStateHelper()) {
        this->csr->getLogicalStateHelper()->writeStreamInline(cmdStream, false);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &cmdStream) {
    CommandListExecutionContext ctx = {};
    programOneCmdListBatchBufferStart(commandList, cmdStream, ctx);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &cmdStream, CommandListExecutionContext &ctx) {
    auto &cmdBufferAllocations = commandList->commandContainer.getCmdBufferAllocations();
    auto cmdBufferCount = cmdBufferAllocations.size();
    bool isCommandListImmediate = (commandList->cmdListType == CommandList::CommandListType::TYPE_IMMEDIATE) ? true : false;

    auto &returnPoints = commandList->getReturnPoints();
    uint32_t returnPointsSize = commandList->getReturnPointsSize();
    uint32_t returnPointIdx = 0;

    for (size_t iter = 0; iter < cmdBufferCount; iter++) {
        auto allocation = cmdBufferAllocations[iter];
        uint64_t startOffset = allocation->getGpuAddress();
        if (isCommandListImmediate && (iter == (cmdBufferCount - 1))) {
            startOffset = ptrOffset(allocation->getGpuAddress(), commandList->commandContainer.currentLinearStreamStartOffset);
        }
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&cmdStream, startOffset, true);
        if (returnPointsSize > 0) {
            bool cmdBufferHasRestarts = std::find_if(
                                            std::next(returnPoints.begin(), returnPointIdx),
                                            returnPoints.end(),
                                            [allocation](CmdListReturnPoint &retPt) {
                                                return retPt.currentCmdBuffer == allocation;
                                            }) != returnPoints.end();
            if (cmdBufferHasRestarts) {
                while (returnPointIdx < returnPointsSize && allocation == returnPoints[returnPointIdx].currentCmdBuffer) {
                    auto scratchSpaceController = this->csr->getScratchSpaceController();
                    ctx.cmdListBeginState.frontEndState.setProperties(returnPoints[returnPointIdx].configSnapshot.frontEndState);
                    programFrontEnd(scratchSpaceController->getScratchPatchAddress(),
                                    scratchSpaceController->getPerThreadScratchSpaceSize(),
                                    cmdStream,
                                    ctx.cmdListBeginState);
                    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&cmdStream,
                                                                                         returnPoints[returnPointIdx].gpuAddress,
                                                                                         true);
                    returnPointIdx++;
                }
            }
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::mergeOneCmdListPipelinedState(CommandList *commandList) {

    bool isCommandListImmediate = (commandList->cmdListType == CommandList::CommandListType::TYPE_IMMEDIATE) ? true : false;
    auto commandListImp = static_cast<CommandListImp *>(commandList);
    if (!isCommandListImmediate && commandListImp->getLogicalStateHelper()) {
        this->csr->getLogicalStateHelper()->mergePipelinedState(*commandListImp->getLogicalStateHelper());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::collectPrintfContentsFromAllCommandsLists(
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists) {

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        this->printfKernelContainer.insert(this->printfKernelContainer.end(),
                                           commandList->getPrintfKernelContainer().begin(),
                                           commandList->getPrintfKernelContainer().end());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::migrateSharedAllocationsIfRequested(
    bool isMigrationRequested,
    ze_command_list_handle_t hCommandList) {

    if (isMigrationRequested) {
        CommandList::fromHandle(hCommandList)->migrateSharedAllocations();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::prefetchMemoryIfRequested(bool &isMemoryPrefetchRequested) {
    if (isMemoryPrefetchRequested) {
        auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        prefetchManager->migrateAllocationsToGpu(*this->device->getDriverHandle()->getSvmAllocsManager(),
                                                 *this->device->getNEODevice());
        isMemoryPrefetchRequested = false;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::assignCsrTaskCountToFenceIfAvailable(ze_fence_handle_t hFence) {
    if (hFence) {
        Fence::fromHandle(hFence)->assignTaskCountFromCsr();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchTaskCountPostSyncByMiFlushDw(
    bool isDispatchTaskCountPostSyncRequired,
    NEO::LinearStream &cmdStream) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }

    uint64_t postSyncAddress = this->csr->getTagAllocation()->getGpuAddress();
    uint32_t postSyncData = this->csr->peekTaskCount() + 1;
    const auto &hwInfo = this->device->getHwInfo();

    NEO::MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = this->csr->isUsedNotifyEnableForPostSync();

    NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdStream, postSyncAddress, postSyncData, args, hwInfo);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchTaskCountPostSyncRegular(
    bool isDispatchTaskCountPostSyncRequired,
    NEO::LinearStream &cmdStream) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }

    uint64_t postSyncAddress = this->csr->getTagAllocation()->getGpuAddress();
    uint32_t postSyncData = this->csr->peekTaskCount() + 1;
    const auto &hwInfo = this->device->getHwInfo();

    NEO::PipeControlArgs args;
    args.dcFlushEnable = this->csr->getDcFlushSupport();
    args.workloadPartitionOffset = this->partitionCount > 1;
    args.notifyEnable = this->csr->isUsedNotifyEnableForPostSync();
    NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        cmdStream,
        NEO::PostSyncMode::ImmediateData,
        postSyncAddress,
        postSyncData,
        hwInfo,
        args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeCsrTagAllocationResident() {
    this->csr->makeResident(*this->csr->getTagAllocation());
}

template <GFXCORE_FAMILY gfxCoreFamily>
NEO::SubmissionStatus CommandQueueHw<gfxCoreFamily>::prepareAndSubmitBatchBuffer(
    CommandListExecutionContext &ctx,
    NEO::LinearStream &innerCommandStream) {

    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto &outerCommandStream = this->commandStream;

    void *endingCmd = nullptr;
    if (ctx.isDirectSubmissionEnabled) {
        auto offset = ptrDiff(innerCommandStream.getCpuBase(), outerCommandStream.getCpuBase()) + innerCommandStream.getUsed();
        uint64_t startAddress = outerCommandStream.getGraphicsAllocation()->getGpuAddress() + offset;
        if (NEO::DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
            startAddress = 0;
        }

        endingCmd = innerCommandStream.getSpace(0);
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&innerCommandStream, startAddress, false);
    } else {
        auto buffer = innerCommandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *(MI_BATCH_BUFFER_END *)buffer = GfxFamily::cmdInitBatchBufferEnd;
    }

    if (ctx.isNEODebuggerActive(this->device) || NEO::DebugManager.flags.EnableSWTags.get() || csr->getLogicalStateHelper()) {
        cleanLeftoverMemory(outerCommandStream, innerCommandStream);
    } else if (this->alignedChildStreamPadding) {
        void *paddingPtr = innerCommandStream.getSpace(this->alignedChildStreamPadding);
        memset(paddingPtr, 0, this->alignedChildStreamPadding);
    }

    return submitBatchBuffer(ptrDiff(innerCommandStream.getCpuBase(), outerCommandStream.getCpuBase()),
                             csr->getResidencyAllocations(),
                             endingCmd,
                             ctx.anyCommandListWithCooperativeKernels);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::cleanLeftoverMemory(NEO::LinearStream &outerCommandStream, NEO::LinearStream &innerCommandStream) {

    auto leftoverSpace = outerCommandStream.getUsed() - innerCommandStream.getUsed();
    leftoverSpace -= ptrDiff(innerCommandStream.getCpuBase(), outerCommandStream.getCpuBase());
    if (leftoverSpace > 0) {
        auto memory = innerCommandStream.getSpace(leftoverSpace);
        memset(memory, 0, leftoverSpace);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateTaskCountAndPostSync(bool isDispatchTaskCountPostSyncRequired) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }
    this->taskCount = this->csr->peekTaskCount();
    this->csr->setLatestFlushedTaskCount(this->taskCount);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::waitForCommandQueueCompletionAndCleanHeapContainer() {

    ze_result_t ret = ZE_RESULT_SUCCESS;

    if (this->getSynchronousMode() == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS) {
        if (const auto syncRet = this->synchronize(std::numeric_limits<uint64_t>::max()); syncRet == ZE_RESULT_ERROR_DEVICE_LOST) {
            ret = syncRet;
        }
    } else {
        this->csr->pollForCompletion();
    }
    this->heapContainer.clear();

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::handleSubmissionAndCompletionResults(
    NEO::SubmissionStatus submitRet,
    ze_result_t completionRet) {

    if ((submitRet != NEO::SubmissionStatus::SUCCESS) || (completionRet == ZE_RESULT_ERROR_DEVICE_LOST)) {
        for (auto &gfx : this->csr->getResidencyAllocations()) {
            if (this->csr->peekLatestFlushedTaskCount() == 0) {
                gfx->releaseUsageInOsContext(this->csr->getOsContext().getContextId());
            } else {
                gfx->updateTaskCount(this->csr->peekLatestFlushedTaskCount(), this->csr->getOsContext().getContextId());
            }
        }
        if (completionRet != ZE_RESULT_ERROR_DEVICE_LOST) {
            completionRet = ZE_RESULT_ERROR_UNKNOWN;
        }
        if (submitRet == NEO::SubmissionStatus::OUT_OF_MEMORY) {
            completionRet = ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        if (submitRet == NEO::SubmissionStatus::OUT_OF_HOST_MEMORY) {
            completionRet = ZE_RESULT_ERROR_OUT_OF_HOST_MEMORY;
        }
    }

    return completionRet;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimatePipelineSelectCmdSize() {
    if (!this->pipelineSelectStateTracking) {
        bool gpgpuEnabled = csr->getPreambleSetFlag();
        return !gpgpuEnabled * NEO::PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(device->getHwInfo());
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimatePipelineSelectCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrStateCopy,
                                                                                           const NEO::StreamProperties &cmdListRequired,
                                                                                           const NEO::StreamProperties &cmdListFinal,
                                                                                           bool &gpgpuEnabled) {
    if (!this->pipelineSelectStateTracking) {
        return 0;
    }

    size_t singlePipelineSelectSize = NEO::PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(device->getHwInfo());
    size_t estimatedSize = 0;

    csrStateCopy.pipelineSelect.setProperties(cmdListRequired.pipelineSelect);
    if (!gpgpuEnabled || csrStateCopy.pipelineSelect.isDirty()) {
        estimatedSize += singlePipelineSelectSize;
        gpgpuEnabled = true;
    }

    csrStateCopy.pipelineSelect.setProperties(cmdListFinal.pipelineSelect);

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListPipelineSelect(CommandList *commandList, NEO::LinearStream &commandStream, NEO::StreamProperties &csrState,
                                                                    const NEO::StreamProperties &cmdListRequired, const NEO::StreamProperties &cmdListFinal) {
    if (!this->pipelineSelectStateTracking) {
        return;
    }

    bool preambleSet = csr->getPreambleSetFlag();
    csrState.pipelineSelect.setProperties(cmdListRequired.pipelineSelect);

    if (!preambleSet || csrState.pipelineSelect.isDirty()) {
        bool systolic = csrState.pipelineSelect.systolicMode.value == 1 ? true : false;
        NEO::PipelineSelectArgs args = {
            systolic,
            false,
            false,
            commandList->getSystolicModeSupport()};

        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, args, device->getHwInfo());
        csr->setPreambleSetFlag(true);
    }

    csrState.pipelineSelect.setProperties(cmdListFinal.pipelineSelect);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateScmCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrStateCopy,
                                                                                const NEO::StreamProperties &cmdListRequired,
                                                                                const NEO::StreamProperties &cmdListFinal) {
    if (!this->stateComputeModeTracking) {
        return 0;
    }

    size_t estimatedSize = 0;

    bool isRcs = this->getCsr()->isRcs();
    size_t singleScmCmdSize = NEO::EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(device->getHwInfo(), false, isRcs);

    csrStateCopy.stateComputeMode.setProperties(cmdListRequired.stateComputeMode);
    if (csrStateCopy.stateComputeMode.isDirty()) {
        estimatedSize += singleScmCmdSize;
    }
    csrStateCopy.stateComputeMode.setProperties(cmdListFinal.stateComputeMode);

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programRequiredStateComputeModeForCommandList(CommandList *commandList,
                                                                                  NEO::LinearStream &commandStream,
                                                                                  NEO::StreamProperties &csrState,
                                                                                  const NEO::StreamProperties &cmdListRequired,
                                                                                  const NEO::StreamProperties &cmdListFinal) {
    if (!this->stateComputeModeTracking) {
        return;
    }

    csrState.stateComputeMode.setProperties(cmdListRequired.stateComputeMode);

    if (csrState.stateComputeMode.isDirty()) {
        NEO::PipelineSelectArgs pipelineSelectArgs = {
            !!csrState.pipelineSelect.systolicMode.value,
            false,
            false,
            commandList->getSystolicModeSupport()};

        bool isRcs = this->getCsr()->isRcs();
        NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(commandStream, csrState.stateComputeMode, pipelineSelectArgs,
                                                                                        false, device->getHwInfo(), isRcs, this->getCsr()->getDcFlushSupport(), nullptr);
    }
    csrState.stateComputeMode.setProperties(cmdListFinal.stateComputeMode);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateBaseAddressState(CommandList *lastCommandList) {
    auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(csr);
    auto dsh = lastCommandList->commandContainer.getIndirectHeap(NEO::HeapType::DYNAMIC_STATE);
    if (dsh != nullptr) {
        csrHw->getDshState().updateAndCheck(dsh);
    }

    auto ssh = lastCommandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE);
    if (ssh != nullptr) {
        csrHw->getSshState().updateAndCheck(ssh);
    }
}

} // namespace L0
