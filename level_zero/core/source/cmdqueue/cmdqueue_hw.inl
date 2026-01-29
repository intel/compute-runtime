/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/heap_base_address_model.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/residency_container.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/unified_memory/unified_memory.h"
#include "shared/source/utilities/software_tags_manager.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_cmdlist_execution_context.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/core/source/fence/fence.h"
#include "level_zero/core/source/helpers/error_code_helper_l0.h"

#include "encode_surface_state_args.h"

#include <algorithm>
#include <limits>

namespace L0 {
template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::createFence(const ze_fence_desc_t *desc,
                                                       ze_fence_handle_t *phFence) {
    *phFence = Fence::create(this, desc);
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::processMemAdviseOperations(CommandList *commandList) {
    auto &memAdviseOperations = commandList->getMemAdviseOperations();
    for (auto &operation : memAdviseOperations) {
        commandList->executeMemAdvise(operation.hDevice, operation.ptr, operation.size, operation.advice);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandLists(
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence,
    bool performMigration,
    NEO::LinearStream *parentImmediateCommandlistLinearStream,
    std::unique_lock<std::mutex> *outerLockForIndirect) {

    auto ret = ZE_RESULT_SUCCESS;

    this->device->activateMetricGroups();

    if (NEO::debugManager.flags.DeferStateInitSubmissionToFirstRegularUsage.get() == 1) {
        this->csr->ensurePrimaryCsrInitialized(*this->device->getNEODevice());
    }

    std::unique_lock<NEO::CommandStreamReceiver::MutexType> lockCSR;

    if (parentImmediateCommandlistLinearStream != nullptr) {
        this->startingCmdBuffer = parentImmediateCommandlistLinearStream;
    } else {
        lockCSR = this->csr->obtainUniqueOwnership();
        this->startingCmdBuffer = &this->commandStream;
    }

    auto neoDevice = device->getNEODevice();

    if (NEO::ApiSpecificConfig::isSharedAllocPrefetchEnabled()) {
        auto svmAllocMgr = device->getDriverHandle()->getSvmAllocsManager();
        svmAllocMgr->prefetchSVMAllocs(*neoDevice, *csr);
    }

    registerCsrClient();

    auto scratchController = this->csr->getScratchSpaceController();
    auto globalStatelessHeapAllocation = this->csr->getGlobalStatelessHeapAllocation();
    bool lockScratchController = false;
    if (this->heaplessModeEnabled) {
        auto primaryScratchController = this->csr->getPrimaryScratchSpaceController();
        scratchController = primaryScratchController;

        globalStatelessHeapAllocation = this->csr->getGlobalStatelessHeapAllocation();
        lockScratchController = scratchController != this->csr->getScratchSpaceController();
    }
    auto ctx = CommandListExecutionContext{phCommandLists,
                                           numCommandLists,
                                           this->isCopyOnlyCommandQueue ? NEO::PreemptionMode::Disabled : csr->getPreemptionMode(),
                                           device,
                                           scratchController,
                                           globalStatelessHeapAllocation,
                                           NEO::Debugger::isDebugEnabled(internalUsage),
                                           csr->isProgramActivePartitionConfigRequired(),
                                           performMigration,
                                           csr->getSipSentFlag()};
    ctx.outerLockForIndirect = outerLockForIndirect;
    ctx.pipelineCmdsDispatch |= ctx.isDebugEnabled &&
                                !this->commandQueueDebugCmdsProgrammed &&
                                device->getL0Debugger();
    ctx.lockScratchController = lockScratchController;
    ctx.lockCSR = &lockCSR;
    ctx.regularHeapful = !this->isCopyOnlyCommandQueue && !this->heaplessStateInitEnabled;

    if (this->isCopyOnlyCommandQueue) {
        ret = this->executeCommandListsCopyOnly(ctx, numCommandLists, phCommandLists, hFence, parentImmediateCommandlistLinearStream);
    } else if (this->heaplessStateInitEnabled) {
        ret = this->executeCommandListsRegularHeapless(ctx, numCommandLists, phCommandLists, hFence, parentImmediateCommandlistLinearStream);
    } else {
        ret = this->executeCommandListsRegular(ctx, numCommandLists, phCommandLists, hFence, parentImmediateCommandlistLinearStream);
    }

    if (NEO::debugManager.flags.PauseOnEnqueue.get() != -1) {
        neoDevice->debugExecutionCounter++;
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandListsRegularHeapless(
    CommandListExecutionContext &ctx,
    uint32_t numCommandLists,
    ze_command_list_handle_t *commandListHandles,
    ze_fence_handle_t hFence,
    NEO::LinearStream *parentImmediateCommandlistLinearStream) {

    auto neoDevice = this->device->getNEODevice();
    this->csr->initializeDeviceWithFirstSubmission(*neoDevice);

    ze_result_t retVal = this->setupCmdListsAndContextParams(ctx, commandListHandles, numCommandLists, hFence, parentImmediateCommandlistLinearStream);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    std::unique_lock<std::mutex> lockForIndirect;
    if (ctx.hasIndirectAccess) {
        handleIndirectAllocationResidency(ctx.unifiedMemoryControls, lockForIndirect, ctx.isMigrationRequested);
    }

    if (ctx.cmdListScratchAddressPatchingEnabled == true) {
        this->handleScratchSpaceAndUpdateGSBAStateDirtyFlag(ctx);
    }

    size_t linearStreamSizeEstimate = this->estimateLinearStreamSizeTotal(ctx, commandListHandles, numCommandLists);

    NEO::LinearStream child(nullptr);
    if (const auto ret = this->makeAlignedChildStreamAndSetGpuBase(child, linearStreamSizeEstimate, ctx); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    NEO::LinearStream *streamForDispatch = parentImmediateCommandlistLinearStream ? parentImmediateCommandlistLinearStream : &child;

    this->getGlobalFenceAndMakeItResident();
    this->getWorkPartitionAndMakeItResident();
    this->getGlobalStatelessHeapAndMakeItResident(ctx);
    this->makePreemptionAllocationResidentForModeMidThread(ctx.isDevicePreemptionModeMidThread);
    this->makeSipIsaResidentIfSipKernelUsed(ctx);
    this->makeDebugSurfaceResidentIfNEODebuggerActive(ctx.isNEODebuggerActive);
    this->makeRayTracingBufferResident(neoDevice->getRTMemoryBackedBuffer());
    this->makeSbaTrackingBufferResidentIfL0DebuggerEnabled(ctx.isDebugEnabled);
    this->makeCsrTagAllocationResident();

    if (ctx.pipelineCmdsDispatch) {
        this->programActivePartitionConfig(ctx.isProgramActivePartitionConfigRequired, *streamForDispatch);
        this->programRequiredCacheFlushes(ctx, *streamForDispatch);
    }

    this->retrivePatchPreambleSpace(ctx, *streamForDispatch);

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(commandListHandles[i]);

        ctx.childGpuAddressPositionBeforeDynamicPreamble = (*streamForDispatch).getCurrentGpuAddressPosition();

        this->dispatchPatchPreambleCommandListWaitSync(ctx, commandList);

        this->patchCommands(*commandList, ctx);
        this->programOneCmdListBatchBufferStart(commandList, *streamForDispatch, ctx);

        this->prefetchMemoryToDeviceAssociatedWithCmdList(commandList);
        if (commandList->hasKernelWithAssert()) {
            cmdListWithAssertExecuted.exchange(true);
        }

        this->collectPrintfContentsFromCommandsList(commandList);
        this->dispatchPatchPreambleInOrderNoop(ctx, commandList);
    }

    this->migrateSharedAllocationsIfRequested(ctx.isMigrationRequested, ctx.firstCommandList);
    this->programLastCommandListReturnBbStart(*streamForDispatch, ctx);
    this->dispatchPatchPreambleEnding(ctx);

    if (!ctx.containsParentImmediateStream) {
        retVal = handleNonParentImmediateStream(hFence, ctx, numCommandLists, commandListHandles, streamForDispatch);
        if (retVal != ZE_RESULT_SUCCESS) {
            return retVal;
        }
        retVal = this->waitForCommandQueueCompletion(ctx);
    }

    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandListsRegular(
    CommandListExecutionContext &ctx,
    uint32_t numCommandLists,
    ze_command_list_handle_t *commandListHandles,
    ze_fence_handle_t hFence,
    NEO::LinearStream *parentImmediateCommandlistLinearStream) {

    ze_result_t retVal = this->setupCmdListsAndContextParams(ctx, commandListHandles, numCommandLists, hFence, parentImmediateCommandlistLinearStream);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    std::unique_lock<std::mutex> lockForIndirect;
    if (ctx.hasIndirectAccess) {
        std::unique_lock<std::mutex> &currentLock = ctx.outerLockForIndirect != nullptr ? *ctx.outerLockForIndirect : lockForIndirect;
        handleIndirectAllocationResidency(ctx.unifiedMemoryControls, currentLock, ctx.isMigrationRequested);
    }

    if (this->heaplessModeEnabled == false || ctx.cmdListScratchAddressPatchingEnabled == true) {
        this->handleScratchSpaceAndUpdateGSBAStateDirtyFlag(ctx);
    }
    this->setFrontEndStateProperties(ctx);

    size_t linearStreamSizeEstimate = this->estimateLinearStreamSizeTotal(ctx, commandListHandles, numCommandLists);

    NEO::LinearStream child(nullptr);
    if (const auto ret = this->makeAlignedChildStreamAndSetGpuBase(child, linearStreamSizeEstimate, ctx); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    NEO::LinearStream *streamForDispatch = parentImmediateCommandlistLinearStream ? parentImmediateCommandlistLinearStream : &child;

    auto neoDevice = this->device->getNEODevice();

    this->updateDebugSurfaceState(ctx);
    this->getGlobalFenceAndMakeItResident();
    this->getWorkPartitionAndMakeItResident();
    this->getGlobalStatelessHeapAndMakeItResident(ctx);
    this->makePreemptionAllocationResidentForModeMidThread(ctx.isDevicePreemptionModeMidThread);
    this->makeSipIsaResidentIfSipKernelUsed(ctx);
    this->makeDebugSurfaceResidentIfNEODebuggerActive(ctx.isNEODebuggerActive);
    this->makeRayTracingBufferResident(neoDevice->getRTMemoryBackedBuffer());
    this->makeSbaTrackingBufferResidentIfL0DebuggerEnabled(ctx.isDebugEnabled);
    this->makeCsrTagAllocationResident();

    if (ctx.pipelineCmdsDispatch) {
        this->programRequiredCacheFlushes(ctx, *streamForDispatch);

        this->getTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(*streamForDispatch);
        this->csr->programHardwareContext(*streamForDispatch);

        if (!this->pipelineSelectStateTracking) {
            this->programPipelineSelectIfGpgpuDisabled(*streamForDispatch);
        } else {
            // Setting systolic/pipeline select here for 1st command list is to preserve dispatch order of hw commands
            if (this->stateChanges.size() > 0) {
                auto &firstCmdListWithStateChange = this->stateChanges[0];
                // check first required state change is for the first command list
                if (firstCmdListWithStateChange.cmdListIndex == 0 && firstCmdListWithStateChange.flags.propertyPsDirty) {
                    this->programOneCmdListPipelineSelect(*streamForDispatch, firstCmdListWithStateChange);
                    firstCmdListWithStateChange.flags.propertyPsDirty = false;
                }
            }
        }
        this->programCommandQueueDebugCmdsForDebuggerIfEnabled(ctx.isDebugEnabled, *streamForDispatch);
        if (!this->stateBaseAddressTracking) {
            this->programStateBaseAddressWithGsbaIfDirty(ctx, ctx.firstCommandList, *streamForDispatch);
        }
        if (neoDevice->getDebugger() != nullptr) {
            if (!this->csr->csrSurfaceProgrammed()) {
                NEO::PreemptionHelper::programCsrBaseAddress<GfxFamily>(*streamForDispatch,
                                                                        *neoDevice,
                                                                        neoDevice->getDebugSurface());
                this->csr->setCsrSurfaceProgrammed(true);
            }
        } else {
            this->programCsrBaseAddressIfPreemptionModeInitial(ctx.isPreemptionModeInitial, *streamForDispatch);
        }
        this->programStateSip(ctx.stateSipRequired, *streamForDispatch);
        this->programActivePartitionConfig(ctx.isProgramActivePartitionConfigRequired, *streamForDispatch);
        bool shouldProgramVfe = !frontEndTrackingEnabled() && ctx.frontEndStateDirty;
        this->programFrontEndAndClearDirtyFlag(shouldProgramVfe, ctx, *streamForDispatch, csr->getStreamProperties());

        if (ctx.rtDispatchRequired) {
            auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
            csrHw->dispatchRayTracingStateCommand(*streamForDispatch, *neoDevice);
        }
    }

    this->retrivePatchPreambleSpace(ctx, *streamForDispatch);

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(commandListHandles[i]);

        ctx.childGpuAddressPositionBeforeDynamicPreamble = (*streamForDispatch).getCurrentGpuAddressPosition();

        if (this->stateChanges.size() > this->currentStateChangeIndex) {
            auto &stateChange = this->stateChanges[this->currentStateChangeIndex];
            if (stateChange.cmdListIndex == i) {
                DEBUG_BREAK_IF(commandList != stateChange.commandList);
                this->updateOneCmdListPreemptionModeAndCtxStatePreemption(*streamForDispatch, stateChange);
                this->programOneCmdListPipelineSelect(*streamForDispatch, stateChange);
                this->programOneCmdListFrontEndIfDirty(ctx, *streamForDispatch, stateChange);
                this->programRequiredStateComputeModeForCommandList(*streamForDispatch, stateChange);
                this->programRequiredStateBaseAddressForCommandList(ctx, *streamForDispatch, stateChange);

                this->currentStateChangeIndex++;
            }
        }

        this->dispatchPatchPreambleCommandListWaitSync(ctx, commandList);

        this->patchCommands(*commandList, ctx);
        this->programOneCmdListBatchBufferStart(commandList, *streamForDispatch, ctx);

        this->prefetchMemoryToDeviceAssociatedWithCmdList(commandList);
        if (commandList->hasKernelWithAssert()) {
            cmdListWithAssertExecuted.exchange(true);
        }

        this->collectPrintfContentsFromCommandsList(commandList);
        this->dispatchPatchPreambleInOrderNoop(ctx, commandList);
    }

    this->updateBaseAddressState(ctx.lastCommandList);
    this->migrateSharedAllocationsIfRequested(ctx.isMigrationRequested, ctx.firstCommandList);

    this->programLastCommandListReturnBbStart(*streamForDispatch, ctx);
    this->dispatchPatchPreambleEnding(ctx);

    this->csr->setPreemptionMode(ctx.statePreemption);

    if (!ctx.containsParentImmediateStream) {
        retVal = handleNonParentImmediateStream(hFence, ctx, numCommandLists, commandListHandles, streamForDispatch);
    }

    this->stateChanges.clear();
    this->currentStateChangeIndex = 0;
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }
    if (!ctx.containsParentImmediateStream) {
        return this->waitForCommandQueueCompletion(ctx);
    }
    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::executeCommandListsCopyOnly(
    CommandListExecutionContext &ctx,
    uint32_t numCommandLists,
    ze_command_list_handle_t *phCommandLists,
    ze_fence_handle_t hFence,
    NEO::LinearStream *parentImmediateCommandlistLinearStream) {

    ze_result_t retVal = this->setupCmdListsAndContextParams(ctx, phCommandLists, numCommandLists, hFence, parentImmediateCommandlistLinearStream);
    if (retVal != ZE_RESULT_SUCCESS) {
        return retVal;
    }

    size_t linearStreamSizeEstimate = this->estimateLinearStreamSizeTotal(ctx, phCommandLists, numCommandLists);

    NEO::LinearStream child(nullptr);
    if (const auto ret = this->makeAlignedChildStreamAndSetGpuBase(child, linearStreamSizeEstimate, ctx); ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    NEO::LinearStream *streamForDispatch = parentImmediateCommandlistLinearStream ? parentImmediateCommandlistLinearStream : &child;

    this->getGlobalFenceAndMakeItResident();
    this->makeCsrTagAllocationResident();

    if (ctx.pipelineCmdsDispatch) {
        this->getTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(*streamForDispatch);
        this->csr->programHardwareContext(*streamForDispatch);
    }

    this->retrivePatchPreambleSpace(ctx, *streamForDispatch);

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        ctx.childGpuAddressPositionBeforeDynamicPreamble = (*streamForDispatch).getCurrentGpuAddressPosition();

        this->dispatchPatchPreambleCommandListWaitSync(ctx, commandList);

        this->programOneCmdListBatchBufferStart(commandList, *streamForDispatch, ctx);
        this->prefetchMemoryToDeviceAssociatedWithCmdList(commandList);
        this->dispatchPatchPreambleInOrderNoop(ctx, commandList);
    }

    this->migrateSharedAllocationsIfRequested(ctx.isMigrationRequested, ctx.firstCommandList);

    this->programLastCommandListReturnBbStart(*streamForDispatch, ctx);
    this->dispatchPatchPreambleEnding(ctx);

    if (!ctx.containsParentImmediateStream) {
        retVal = handleNonParentImmediateStream(hFence, ctx, numCommandLists, phCommandLists, streamForDispatch);
        if (retVal != ZE_RESULT_SUCCESS) {
            return retVal;
        }
        retVal = this->waitForCommandQueueCompletion(ctx);
    }
    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListFrontEndIfDirty(
    CommandListExecutionContext &ctx,
    NEO::LinearStream &cmdStream,
    CommandListRequiredStateChange &cmdListRequired) {

    if (!frontEndTrackingEnabled()) {
        return;
    }

    if (cmdListRequired.flags.propertyFeDirty) {
        this->programFrontEndAndClearDirtyFlag(cmdListRequired.flags.propertyFeDirty, ctx, cmdStream, cmdListRequired.requiredState);
    }

    if (cmdListRequired.flags.frontEndReturnPoint) {
        ctx.cmdListBeginState.frontEndState.resetState();
        ctx.cmdListBeginState.frontEndState.copyPropertiesAll(cmdListRequired.requiredState.frontEndState);
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
    programFrontEnd(ctx.scratchSpaceController->getScratchPatchAddress(),
                    ctx.scratchSpaceController->getPerThreadScratchSpaceSizeSlot0(),
                    cmdStream,
                    csrState);
    ctx.frontEndStateDirty = false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSlot0Size, NEO::LinearStream &cmdStream, NEO::StreamProperties &streamProperties) {
    UNRECOVERABLE_IF(csr == nullptr);
    auto &hwInfo = device->getHwInfo();
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto engineGroupType = gfxCoreHelper.getEngineGroupType(csr->getOsContext().getEngineType(),
                                                            csr->getOsContext().getEngineUsage(), hwInfo);
    auto feState = NEO::PreambleHelper<GfxFamily>::getSpaceForVfeState(&cmdStream, hwInfo, engineGroupType, nullptr);
    NEO::PreambleHelper<GfxFamily>::programVfeState(feState,
                                                    device->getNEODevice()->getRootDeviceEnvironment(),
                                                    perThreadScratchSpaceSlot0Size,
                                                    scratchAddress,
                                                    device->getMaxNumHwThreads(),
                                                    streamProperties);
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
    bool &isFrontEndStateDirty, CommandList *commandList,
    NEO::StreamProperties &csrState,
    const NEO::StreamProperties &cmdListRequired,
    const NEO::StreamProperties &cmdListFinal,
    NEO::StreamProperties &requiredState,
    bool &propertyDirty,
    bool &frontEndReturnPoint) {

    if (!frontEndTrackingEnabled()) {
        return 0;
    }

    auto singleFrontEndCmdSize = estimateFrontEndCmdSize();
    size_t estimatedSize = 0;
    bool feCurrentDirty = isFrontEndStateDirty;

    if (isFrontEndStateDirty) {
        csrState.frontEndState.copyPropertiesAll(cmdListRequired.frontEndState);
        csrState.frontEndState.setPropertySingleSliceDispatchCcsMode();
    } else {
        csrState.frontEndState.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(cmdListRequired.frontEndState);
        feCurrentDirty = csrState.frontEndState.isDirty();
    }

    if (feCurrentDirty) {
        estimatedSize += singleFrontEndCmdSize;

        propertyDirty = true;
    }

    uint32_t frontEndChanges = commandList->getReturnPointsSize();
    if (frontEndChanges > 0) {
        estimatedSize += (frontEndChanges * singleFrontEndCmdSize);
        estimatedSize += (frontEndChanges * NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize());
        frontEndReturnPoint = true;
    }

    if (frontEndReturnPoint || propertyDirty) {
        requiredState.frontEndState = csrState.frontEndState;
    }

    if (isFrontEndStateDirty) {
        csrState.frontEndState.copyPropertiesAll(cmdListFinal.frontEndState);
        isFrontEndStateDirty = false;
    } else {
        csrState.frontEndState.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(cmdListFinal.frontEndState);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programPipelineSelectIfGpgpuDisabled(NEO::LinearStream &cmdStream) {
    bool gpgpuEnabled = this->csr->getPreambleSetFlag();
    if (!gpgpuEnabled) {
        NEO::PipelineSelectArgs args = {false, false, false};
        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(&cmdStream, args, device->getNEODevice()->getRootDeviceEnvironment());
        this->csr->setPreambleSetFlag(true);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::isDispatchTaskCountPostSyncRequired(ze_fence_handle_t hFence, bool containsAnyRegularCmdList, bool containsParentImmediateStream) const {
    return (!containsParentImmediateStream) && (containsAnyRegularCmdList || !csr->isUpdateTagFromWaitEnabled() || hFence != nullptr || isSynchronousMode());
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::getPreemptionCmdProgramming() {
    return NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(NEO::PreemptionMode::MidThread, NEO::PreemptionMode::Initial) > 0u;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::computeDebuggerCmdsSize(const CommandListExecutionContext &ctx) {
    size_t debuggerCmdsSize = 0;

    if (ctx.isDebugEnabled && !this->commandQueueDebugCmdsProgrammed) {
        if (this->device->getL0Debugger()) {
            debuggerCmdsSize += device->getL0Debugger()->getSbaAddressLoadCommandsSize();
        }
    }

    return debuggerCmdsSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::computePreemptionSizeForCommandList(
    CommandListExecutionContext &ctx,
    CommandList *commandList,
    bool &dirtyState) {

    size_t preemptionSize = 0u;

    auto commandListPreemption = commandList->getCommandListPreemptionMode();

    if (ctx.statePreemption != commandListPreemption) {
        if (this->preemptionCmdSyncProgramming) {
            preemptionSize += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        }
        preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(commandListPreemption, ctx.statePreemption);
        ctx.statePreemption = commandListPreemption;
        dirtyState = true;
    }

    return preemptionSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::setupCmdListsAndContextParams(
    CommandListExecutionContext &ctx,
    ze_command_list_handle_t *phCommandLists,
    uint32_t numCommandLists,
    ze_fence_handle_t hFence,
    NEO::LinearStream *parentImmediateCommandlistLinearStream) {

    auto neoDevice = this->device->getNEODevice();
    ctx.containsAnyRegularCmdList = !ctx.firstCommandList->isImmediateType();

    if (!isCopyOnlyCommandQueue) {
        ctx.isDirectSubmissionEnabled = this->csr->isDirectSubmissionEnabled();
        ctx.instructionCacheFlushRequired = this->csr->isInstructionCacheFlushRequired();
        ctx.stateCacheFlushRequired = neoDevice->getBindlessHeapsHelper() ? neoDevice->getBindlessHeapsHelper()->getStateDirtyForContext(this->csr->getOsContext().getContextId()) : false;
        ctx.pipelineCmdsDispatch |= (ctx.instructionCacheFlushRequired || ctx.stateCacheFlushRequired);
    } else {
        ctx.isDirectSubmissionEnabled = this->csr->isBlitterDirectSubmissionEnabled();
    }

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = static_cast<CommandListImp *>(CommandList::fromHandle(phCommandLists[i]));
        if (!commandList->isClosed()) {
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
        this->processMemAdviseOperations(commandList);

        commandList->storeReferenceTsToMappedEvents(false);

        this->prepareInOrderCommandList(commandList, ctx);

        commandList->setInterruptEventsCsr(*this->csr);

        auto &commandContainer = commandList->getCmdContainer();

        if (!isCopyOnlyCommandQueue) {
            ctx.perThreadScratchSpaceSlot0Size = std::max(ctx.perThreadScratchSpaceSlot0Size, commandList->getCommandListPerThreadScratchSize(0u));
            ctx.perThreadScratchSpaceSlot1Size = std::max(ctx.perThreadScratchSpaceSlot1Size, commandList->getCommandListPerThreadScratchSize(1u));

            if (commandList->getCmdListHeapAddressModel() == NEO::HeapAddressModel::privateHeaps) {
                if (commandList->getCommandListPerThreadScratchSize(0u) != 0 || commandList->getCommandListPerThreadScratchSize(1u) != 0) {
                    if (commandContainer.getIndirectHeap(NEO::HeapType::surfaceState) != nullptr) {
                        heapContainer.push_back(commandContainer.getIndirectHeap(NEO::HeapType::surfaceState)->getGraphicsAllocation());
                    }
                    for (auto &element : commandContainer.getSshAllocations()) {
                        heapContainer.push_back(element);
                    }
                }
            }

            if (commandList->containsCooperativeKernels()) {
                ctx.anyCommandListWithCooperativeKernels = true;
            }

            if (commandList->getRequiredStreamState().frontEndState.disableEUFusion.value == 1) {
                ctx.anyCommandListRequiresDisabledEUFusion = true;
            }

            // If the Command List has commands that require uncached MOCS, then any changes to the commands in the queue requires the uncached MOCS
            if (commandList->isRequiredQueueUncachedMocs() && ctx.cachedMOCSAllowed == true) {
                ctx.cachedMOCSAllowed = false;
            }

            ctx.hasIndirectAccess |= commandList->hasIndirectAllocationsAllowed();
            if (commandList->hasIndirectAllocationsAllowed()) {
                ctx.unifiedMemoryControls.indirectDeviceAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectDeviceAllocationsAllowed;
                ctx.unifiedMemoryControls.indirectHostAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectHostAllocationsAllowed;
                ctx.unifiedMemoryControls.indirectSharedAllocationsAllowed |= commandList->getUnifiedMemoryControls().indirectSharedAllocationsAllowed;
            }

            this->partitionCount = std::max(this->partitionCount, commandList->getPartitionCount());

            ctx.cmdListScratchAddressPatchingEnabled |= commandList->getCmdListScratchAddressPatchingEnabled();

            ctx.spaceForResidency += estimateCommandListResidencySize(commandList);
        } else {
            ctx.taskCountUpdateFenceRequired |= commandList->isTaskCountUpdateFenceRequired();
        }

        this->isWalkerWithProfilingEnqueued = commandList->getIsWalkerWithProfilingEnqueued();
    }

    this->getCsr()->getResidencyAllocations().reserve(ctx.spaceForResidency);

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        makeResidentAndMigrate(ctx.isMigrationRequested, commandList->getCmdContainer().getResidencyContainer());
    }

    if (parentImmediateCommandlistLinearStream) {
        ctx.containsParentImmediateStream = true;
    }
    ctx.isDispatchTaskCountPostSyncRequired = isDispatchTaskCountPostSyncRequired(hFence, ctx.containsAnyRegularCmdList,
                                                                                  ctx.containsParentImmediateStream);

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeTotal(CommandListExecutionContext &ctx,
                                                                    ze_command_list_handle_t *commandListHandles,
                                                                    uint32_t numCommandLists) {
    size_t linearStreamSizeEstimate = 0u;

    linearStreamSizeEstimate = estimateLinearStreamSizeSharedInitial(ctx);

    EstimateRegularHeapfulPerCmdlistData regularPerCmdlistData;
    if (ctx.regularHeapful) {
        regularPerCmdlistData.frontEndStateDirty = ctx.frontEndStateDirty;
        regularPerCmdlistData.gpgpuEnabled = this->csr->getPreambleSetFlag();
        regularPerCmdlistData.baseAddressStateDirty = ctx.gsbaStateDirty;
        regularPerCmdlistData.scmStateDirty = this->csr->getStateComputeModeDirty();

        ctx.pipelineCmdsDispatch |= !regularPerCmdlistData.gpgpuEnabled;
        ctx.pipelineCmdsDispatch |= regularPerCmdlistData.scmStateDirty;
    }
    auto &csrStreamProperties = this->csr->getStreamProperties();

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(commandListHandles[i]);

        linearStreamSizeEstimate += estimateLinearStreamSizeSharedPerCmdList(ctx, commandList);
        if (ctx.regularHeapful) {
            linearStreamSizeEstimate += estimateLinearStreamSizeRegularHeapfulPerCmdList(ctx, commandList, i, regularPerCmdlistData, csrStreamProperties);
        }
    }

    if (ctx.regularHeapful) {
        linearStreamSizeEstimate += estimateLinearStreamSizeRegularHeapfulPostCmdList(ctx);
    }

    linearStreamSizeEstimate += estimateLinearStreamSizeSharedPostCmdList(ctx, numCommandLists);

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeSharedPerCmdList(CommandListExecutionContext &ctx,
                                                                               CommandList *commandList) {
    size_t linearStreamSizeEstimate = 0u;
    linearStreamSizeEstimate += estimateCommandListSecondaryStart(commandList);

    // per command list patch preamble
    getCommandListPatchPreambleData(ctx, commandList);
    linearStreamSizeEstimate += estimateCommandListPatchPreambleFrontEndCmd(ctx, commandList);
    linearStreamSizeEstimate += estimateCommandListPatchPreambleWaitSync(ctx, commandList);
    linearStreamSizeEstimate += estimateCommandListPatchPreambleHostFunctions(ctx, commandList);

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeSharedInitial(
    CommandListExecutionContext &ctx) {

    size_t linearStreamSizeEstimate = 0u;

    if (this->isCopyOnlyCommandQueue || !this->heaplessStateInitEnabled) {
        auto hwContextSizeEstimate = this->csr->getCmdsSizeForHardwareContext();
        if (hwContextSizeEstimate > 0) {
            linearStreamSizeEstimate += hwContextSizeEstimate;
            ctx.pipelineCmdsDispatch = true;
        }
    }

    if (ctx.isDirectSubmissionEnabled) {
        linearStreamSizeEstimate += NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
        if (NEO::debugManager.flags.DirectSubmissionRelaxedOrdering.get() == 1) {
            linearStreamSizeEstimate += 2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_REG);
        }
    } else {
        linearStreamSizeEstimate += NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferEndSize();
    }

    auto csrHw = reinterpret_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
    if (ctx.isProgramActivePartitionConfigRequired) {
        linearStreamSizeEstimate += csrHw->getCmdSizeForActivePartitionConfig();
    }

    if (NEO::debugManager.flags.EnableSWTags.get()) {
        linearStreamSizeEstimate += NEO::SWTagsManager::estimateSpaceForSWTags<GfxFamily>();
        ctx.pipelineCmdsDispatch = true;
    }

    if (ctx.isDispatchTaskCountPostSyncRequired) {
        if (this->isCopyOnlyCommandQueue) {
            NEO::EncodeDummyBlitWaArgs waArgs{false, &(this->device->getNEODevice()->getRootDeviceEnvironmentRef())};
            linearStreamSizeEstimate += NEO::EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
            if (ctx.taskCountUpdateFenceRequired) {
                linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, device->getNEODevice()->getRootDeviceEnvironment());
            }
        } else {
            linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(this->device->getNEODevice()->getRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
        }
    }

    if (ctx.instructionCacheFlushRequired) {
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForInstructionCacheFlush();
    }

    if (ctx.stateCacheFlushRequired) {
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListSecondaryStart(CommandList *commandList) {
    if (!this->dispatchCmdListBatchBufferAsPrimary) {
        return (commandList->getCmdContainer().getCmdBufferAllocations().size() * NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize());
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListPrimaryStart(bool required) {
    if (this->dispatchCmdListBatchBufferAsPrimary && required) {
        return NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListPatchPreambleFrontEndCmd(CommandListExecutionContext &ctx, CommandList *commandList) {
    size_t encodeSize = 0;
    if (this->patchingPreamble) {
        uint32_t feCmdCount = commandList->getFrontEndPatchListCount();
        if (feCmdCount > 0) {
            const size_t feCmdSize = NEO::PreambleHelper<GfxFamily>::getVFECommandsSize();
            size_t singleFeCmdEncodeSize = NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(feCmdSize);

            encodeSize = singleFeCmdEncodeSize * feCmdCount;
            ctx.bufferSpaceForPatchPreamble += encodeSize;
        }
    }
    return encodeSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListPatchPreambleWaitSync(CommandListExecutionContext &ctx, CommandList *commandList) {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
    size_t waitSize = 0;
    if (this->patchingPreamble && this->saveWaitForPreamble) {
        uint64_t tagGpuAddress = commandList->getLatestTagGpuAddress();
        ctx.patchPreambleWaitSyncNeeded = (tagGpuAddress != 0) && (getCsr()->getTagAllocation()->getGpuAddress() != tagGpuAddress);
        if (ctx.patchPreambleWaitSyncNeeded) {
            waitSize = NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
            waitSize += (2 * sizeof(MI_LOAD_REGISTER_IMM));
        }
        ctx.bufferSpaceForPatchPreamble += waitSize;
    }
    return waitSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListPatchPreambleHostFunctions(CommandListExecutionContext &ctx, CommandList *commandList) {
    using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    size_t encodeSize = 0;
    if (this->patchingPreamble) {

        bool dcFlushRequired = csr->getDcFlushSupport();
        bool usePipeControlForHostFunctionIdProgramming = NEO::HostFunctionHelper<GfxFamily>::usePipeControlForHostFunction(dcFlushRequired);

        uint32_t hostFunctionsCount = commandList->getHostFunctionPatchListCount();
        if (hostFunctionsCount > 0) {
            auto sizeToEncodeId = usePipeControlForHostFunctionIdProgramming ? sizeof(PIPE_CONTROL) : sizeof(MI_STORE_DATA_IMM);
            auto semaphoreSize = NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
            auto encodedHostFunctionIdCmdSize = NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(sizeToEncodeId);
            auto encodedMiSemaphoreSize = NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(semaphoreSize);

            auto encodedHostFuncCmdSize = encodedHostFunctionIdCmdSize + (this->partitionCount * encodedMiSemaphoreSize);
            encodeSize = encodedHostFuncCmdSize * hostFunctionsCount;
            ctx.bufferSpaceForPatchPreamble += encodeSize;
        }
    }

    return encodeSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline size_t CommandQueueHw<gfxCoreFamily>::estimateTotalCommandListPatchPreambleData(CommandListExecutionContext &ctx, uint32_t numCommandLists) {
    size_t encodeSize = 0;
    if (this->patchingPreamble) {
        constexpr size_t bbStartSize = NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
        size_t singleBbStartEncodeSize = NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(bbStartSize);
        encodeSize = singleBbStartEncodeSize * numCommandLists;

        // barrier command to pause between patch preamble completion and execution of command lists
        encodeSize += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        encodeSize += 2 * NEO::EncodeMiArbCheck<GfxFamily>::getCommandSize();

        if (ctx.totalNoopSpaceForPatchPreamble > 0) {
            encodeSize += NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(ctx.totalNoopSpaceForPatchPreamble);
        }

        if (ctx.totalActiveScratchPatchElements > 0) {
            const size_t qwordEncodeSize = NEO::EncodeDataMemory<GfxFamily>::getCommandSizeForEncode(sizeof(uint64_t));
            size_t patchScratchElemsEncodeSize = qwordEncodeSize * ctx.totalActiveScratchPatchElements;

            encodeSize += patchScratchElemsEncodeSize;
        }
        ctx.bufferSpaceForPatchPreamble += encodeSize;

        // patch preamble dispatched into queue's buffer forces not to use cmdlist as a starting buffer
        this->forceBbStartJump = true;
    }
    return encodeSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
inline void CommandQueueHw<gfxCoreFamily>::getCommandListPatchPreambleData(CommandListExecutionContext &ctx, CommandList *commandList) {
    if (this->patchingPreamble) {
        ctx.totalNoopSpaceForPatchPreamble += commandList->getTotalNoopSpace();
        ctx.totalActiveScratchPatchElements += commandList->getActiveScratchPatchElements();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::retrivePatchPreambleSpace(CommandListExecutionContext &ctx, NEO::LinearStream &commandStream) {
    if (this->patchingPreamble) {
        ctx.currentPatchPreambleBuffer = commandStream.getSpace(ctx.bufferSpaceForPatchPreamble);
        ctx.basePatchPreambleAddress = reinterpret_cast<uintptr_t>(ctx.currentPatchPreambleBuffer);

        NEO::EncodeMiArbCheck<GfxFamily>::program(ctx.currentPatchPreambleBuffer, true);
        ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, NEO::EncodeMiArbCheck<GfxFamily>::getCommandSize());
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchPatchPreambleEnding(CommandListExecutionContext &ctx) {
    if (this->patchingPreamble) {
        NEO::PipeControlArgs args;

        NEO::MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(ctx.currentPatchPreambleBuffer, args);
        ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier());

        NEO::EncodeMiArbCheck<GfxFamily>::program(ctx.currentPatchPreambleBuffer, false);
        ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, NEO::EncodeMiArbCheck<GfxFamily>::getCommandSize());

        auto currentPatchPreambleAddress = reinterpret_cast<uintptr_t>(ctx.currentPatchPreambleBuffer);
        uintptr_t estimatedEndPreambleAddress = ctx.basePatchPreambleAddress + ctx.bufferSpaceForPatchPreamble;
        if (estimatedEndPreambleAddress > currentPatchPreambleAddress) {
            memset(ctx.currentPatchPreambleBuffer, 0, (estimatedEndPreambleAddress - currentPatchPreambleAddress));
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchPatchPreambleInOrderNoop(CommandListExecutionContext &ctx, CommandList *commandList) {
    auto commandListImp = static_cast<CommandListImp *>(commandList);
    if (this->patchingPreamble && commandListImp->isInOrderExecutionEnabled()) {
        auto deviceNodeGpuAddress = commandListImp->getInOrderExecDeviceGpuAddress();
        UNRECOVERABLE_IF(deviceNodeGpuAddress == 0u);
        NEO::EncodeDataMemory<GfxFamily>::programNoop(ctx.currentPatchPreambleBuffer, deviceNodeGpuAddress, commandListImp->getInOrderExecDeviceRequiredSize());

        auto hostNodeGpuAddress = commandListImp->getInOrderExecHostGpuAddress();
        if (hostNodeGpuAddress != 0) {
            NEO::EncodeDataMemory<GfxFamily>::programNoop(ctx.currentPatchPreambleBuffer, hostNodeGpuAddress, commandListImp->getInOrderExecHostRequiredSize());
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchPatchPreambleCommandListWaitSync(CommandListExecutionContext &ctx, CommandList *commandList) {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    if (this->patchingPreamble) {
        if (ctx.patchPreambleWaitSyncNeeded) {
            constexpr uint32_t firstRegister = RegisterOffsets::csGprR0;
            constexpr uint32_t secondRegister = RegisterOffsets::csGprR0 + 4;

            auto waitValue = commandList->getLatestTaskCount();

            NEO::LriHelper<GfxFamily>::program(reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ctx.currentPatchPreambleBuffer),
                                               firstRegister,
                                               getLowPart(waitValue),
                                               true,
                                               this->isCopyOnlyCommandQueue);
            ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, sizeof(MI_LOAD_REGISTER_IMM));

            NEO::LriHelper<GfxFamily>::program(reinterpret_cast<MI_LOAD_REGISTER_IMM *>(ctx.currentPatchPreambleBuffer),
                                               secondRegister,
                                               getHighPart(waitValue),
                                               true,
                                               this->isCopyOnlyCommandQueue);
            ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, sizeof(MI_LOAD_REGISTER_IMM));

            bool useSemaphore64bCmd = device->getNEODevice()->getDeviceInfo().semaphore64bCmdSupport;
            NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(ctx.currentPatchPreambleBuffer),
                                                                    commandList->getLatestTagGpuAddress(),
                                                                    commandList->getLatestTaskCount(),
                                                                    COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD,
                                                                    false,
                                                                    true,
                                                                    GfxFamily::isQwordInOrderCounter,
                                                                    GfxFamily::isQwordInOrderCounter,
                                                                    false,
                                                                    useSemaphore64bCmd);
            ctx.currentPatchPreambleBuffer = ptrOffset(ctx.currentPatchPreambleBuffer, NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait());

            this->csr->makeResident(*commandList->getLatestTagGpuAllocation());
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateCommandListResidencySize(CommandList *commandList) {
    return commandList->getCmdContainer().getResidencyContainer().size();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::setFrontEndStateProperties(CommandListExecutionContext &ctx) {

    auto &streamProperties = this->csr->getStreamProperties();
    if (!frontEndTrackingEnabled()) {
        streamProperties.frontEndState.setPropertiesAll(ctx.anyCommandListWithCooperativeKernels, ctx.anyCommandListRequiresDisabledEUFusion,
                                                        true);
        ctx.frontEndStateDirty |= streamProperties.frontEndState.isDirty();
    }
    ctx.frontEndStateDirty |= csr->getMediaVFEStateDirty();
    ctx.pipelineCmdsDispatch |= ctx.frontEndStateDirty;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::handleScratchSpaceAndUpdateGSBAStateDirtyFlag(CommandListExecutionContext &ctx) {
    std::unique_lock<NEO::CommandStreamReceiver::MutexType> defaultCsrLock;
    if (ctx.lockScratchController) {
        defaultCsrLock = device->getNEODevice()->getDefaultEngine().commandStreamReceiver->obtainUniqueOwnership();
    }

    bool localGsbaDirty = false;
    bool localFrontEndDirty = false;
    handleScratchSpace(this->heapContainer,
                       ctx.scratchSpaceController,
                       ctx.globalStatelessAllocation,
                       localGsbaDirty, localFrontEndDirty,
                       ctx.perThreadScratchSpaceSlot0Size, ctx.perThreadScratchSpaceSlot1Size);

    if (this->heaplessModeEnabled == false) {
        ctx.gsbaStateDirty |= localGsbaDirty;
        ctx.frontEndStateDirty |= localFrontEndDirty;

        ctx.gsbaStateDirty |= this->csr->getGSBAStateDirty();
        ctx.pipelineCmdsDispatch |= ctx.gsbaStateDirty;
    }
    ctx.scratchGsba = ctx.scratchSpaceController->calculateNewGSH();
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeRegularHeapfulPerCmdList(
    CommandListExecutionContext &ctx,
    CommandList *commandList,
    uint32_t index,
    EstimateRegularHeapfulPerCmdlistData &sharedData,
    NEO::StreamProperties &csrStreamProperties) {

    size_t linearStreamSizeEstimate = 0u;

    const NEO::StreamProperties &requiredStreamState = commandList->getRequiredStreamState();
    const NEO::StreamProperties &finalStreamState = commandList->getFinalStreamState();

    linearStreamSizeEstimate += estimateFrontEndCmdSizeForMultipleCommandLists(sharedData.frontEndStateDirty, commandList,
                                                                               csrStreamProperties, requiredStreamState, finalStreamState,
                                                                               sharedData.cmdListState.requiredState,
                                                                               sharedData.cmdListState.flags.propertyFeDirty, sharedData.cmdListState.flags.frontEndReturnPoint);
    linearStreamSizeEstimate += estimatePipelineSelectCmdSizeForMultipleCommandLists(csrStreamProperties, requiredStreamState, finalStreamState, sharedData.gpgpuEnabled,
                                                                                     sharedData.cmdListState.requiredState, sharedData.cmdListState.flags.propertyPsDirty);
    linearStreamSizeEstimate += estimateScmCmdSizeForMultipleCommandLists(csrStreamProperties, sharedData.scmStateDirty, requiredStreamState, finalStreamState,
                                                                          sharedData.cmdListState.requiredState, sharedData.cmdListState.flags.propertyScmDirty);
    linearStreamSizeEstimate += estimateStateBaseAddressCmdSizeForMultipleCommandLists(sharedData.baseAddressStateDirty, commandList->getCmdListHeapAddressModel(), csrStreamProperties, requiredStreamState, finalStreamState,
                                                                                       sharedData.cmdListState.requiredState, sharedData.cmdListState.flags.propertySbaDirty);
    linearStreamSizeEstimate += computePreemptionSizeForCommandList(ctx, commandList, sharedData.cmdListState.flags.preemptionDirty);

    if (sharedData.cmdListState.flags.isAnyDirty()) {
        sharedData.cmdListState.commandList = commandList;
        sharedData.cmdListState.cmdListIndex = index;
        sharedData.cmdListState.newPreemptionMode = ctx.statePreemption;
        this->stateChanges.push_back(sharedData.cmdListState);

        linearStreamSizeEstimate += this->estimateCommandListPrimaryStart(true);

        sharedData.cmdListState.requiredState.resetState();
        sharedData.cmdListState.flags.cleanDirty();
    }

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeRegularHeapfulPostCmdList(CommandListExecutionContext &ctx) {
    size_t linearStreamSizeEstimate = 0u;

    linearStreamSizeEstimate += estimateFrontEndCmdSize(ctx.frontEndStateDirty);
    linearStreamSizeEstimate += estimatePipelineSelectCmdSize();
    if (ctx.gsbaStateDirty && !this->stateBaseAddressTracking) {
        linearStreamSizeEstimate += estimateStateBaseAddressCmdSize();
    }

    NEO::Device *neoDevice = this->device->getNEODevice();

    if (csr->isRayTracingStateProgramingNeeded(*neoDevice)) {
        ctx.rtDispatchRequired = true;
        auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(this->csr);
        linearStreamSizeEstimate += csrHw->getCmdSizeForPerDssBackedBuffer(this->device->getHwInfo());

        ctx.pipelineCmdsDispatch = true;
    }

    if (neoDevice->getDebugger() != nullptr) {
        if (!this->csr->csrSurfaceProgrammed()) {
            linearStreamSizeEstimate += NEO::PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*neoDevice);
        }
    } else if (ctx.isPreemptionModeInitial) {
        linearStreamSizeEstimate += NEO::PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*neoDevice);
    }
    if (ctx.stateSipRequired) {
        linearStreamSizeEstimate += NEO::PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*neoDevice, this->csr->isRcs());
    }

    linearStreamSizeEstimate += this->computeDebuggerCmdsSize(ctx);

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateLinearStreamSizeSharedPostCmdList(CommandListExecutionContext &ctx, uint32_t numCommandLists) {
    size_t linearStreamSizeEstimate = 0u;

    linearStreamSizeEstimate += this->estimateTotalCommandListPatchPreambleData(ctx, numCommandLists);

    bool additionalCondition = true;
    if (ctx.regularHeapful) {
        bool firstCmdlistDynamicPreamble = (this->stateChanges.size() > 0 && this->stateChanges[0].cmdListIndex == 0);
        additionalCondition = !firstCmdlistDynamicPreamble;
    }
    bool bbStartNeeded = additionalCondition && (ctx.pipelineCmdsDispatch || this->forceBbStartJump);
    linearStreamSizeEstimate += this->estimateCommandListPrimaryStart(bbStartNeeded);

    return linearStreamSizeEstimate;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::makeAlignedChildStreamAndSetGpuBase(NEO::LinearStream &child, size_t requiredSize, CommandListExecutionContext &ctx) {
    if (ctx.containsParentImmediateStream) {
        return ZE_RESULT_SUCCESS;
    }

    size_t alignedSize = alignUp<size_t>(requiredSize, this->minCmdBufferPtrAlign);

    if (const auto waitStatus = this->reserveLinearStreamSize(alignedSize); waitStatus == NEO::WaitStatus::gpuHang) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    child.replaceBuffer(this->commandStream.getSpace(alignedSize), alignedSize);
    child.setGpuBase(ptrOffset(this->commandStream.getGpuBase(), this->commandStream.getUsed() - alignedSize));
    this->alignedChildStreamPadding = alignedSize - requiredSize;
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::getGlobalFenceAndMakeItResident() {
    const auto globalFenceAllocation = this->csr->getGlobalFenceAllocation();
    if (globalFenceAllocation) {
        this->csr->makeResident(*globalFenceAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::getWorkPartitionAndMakeItResident() {
    const auto workPartitionAllocation = this->csr->getWorkPartitionAllocation();
    if (workPartitionAllocation) {
        this->csr->makeResident(*workPartitionAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::getGlobalStatelessHeapAndMakeItResident(CommandListExecutionContext &ctx) {
    if (ctx.globalStatelessAllocation) {
        this->csr->makeResident(*ctx.globalStatelessAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::getTagsManagerHeapsAndMakeThemResidentIfSWTagsEnabled(NEO::LinearStream &cmdStream) {
    if (NEO::debugManager.flags.EnableSWTags.get()) {
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
void CommandQueueHw<gfxCoreFamily>::programCommandQueueDebugCmdsForDebuggerIfEnabled(bool isDebugEnabled, NEO::LinearStream &cmdStream) {
    if (isDebugEnabled && !this->commandQueueDebugCmdsProgrammed) {
        if (this->device->getL0Debugger()) {
            this->device->getL0Debugger()->programSbaAddressLoad(cmdStream,
                                                                 device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId())->getGpuAddress(),
                                                                 NEO::EngineHelpers::isBcs(this->csr->getOsContext().getEngineType()));
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
    auto indirectHeap = CommandList::fromHandle(hCommandList)->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    programStateBaseAddress(ctx.scratchGsba,
                            indirectHeap->getGraphicsAllocation()->isAllocatedInLocalMemoryPool(),
                            cmdStream,
                            ctx.cachedMOCSAllowed,
                            nullptr);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programCsrBaseAddressIfPreemptionModeInitial(bool isPreemptionModeInitial, NEO::LinearStream &cmdStream) {
    if (!isPreemptionModeInitial) {
        return;
    }
    NEO::Device *neoDevice = this->device->getNEODevice();
    NEO::PreemptionHelper::programCsrBaseAddress<GfxFamily>(cmdStream,
                                                            *neoDevice,
                                                            this->csr->getPreemptionAllocation());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programStateSip(bool isStateSipRequired, NEO::LinearStream &cmdStream) {
    if (!isStateSipRequired) {
        return;
    }
    NEO::Device *neoDevice = this->device->getNEODevice();
    NEO::PreemptionHelper::programStateSip<GfxFamily>(cmdStream, *neoDevice, &this->csr->getOsContext());
    this->csr->setSipSentFlag(true);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateOneCmdListPreemptionModeAndCtxStatePreemption(
    NEO::LinearStream &cmdStream,
    CommandListRequiredStateChange &cmdListRequired) {

    if (cmdListRequired.flags.preemptionDirty) {
        if (NEO::debugManager.flags.EnableSWTags.get()) {
            NEO::Device *neoDevice = this->device->getNEODevice();
            neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::PipeControlReasonTag>(
                cmdStream,
                *neoDevice,
                "CommandList Preemption Mode update", 0u);
        }
        if (this->preemptionCmdSyncProgramming) {
            NEO::PipeControlArgs args;
            NEO::MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);
        }
        NEO::PreemptionHelper::programCmdStream<GfxFamily>(cmdStream,
                                                           cmdListRequired.newPreemptionMode,
                                                           NEO::PreemptionMode::Initial,
                                                           this->csr->getPreemptionAllocation());
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
    if (ctx.isDevicePreemptionModeMidThread || ctx.isNEODebuggerActive) {

        NEO::GraphicsAllocation *sipAllocation = NEO::SipKernel::getSipKernel(*neoDevice, &this->csr->getOsContext()).getSipAllocation();
        this->csr->makeResident(*sipAllocation);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeDebugSurfaceResidentIfNEODebuggerActive(bool isNEODebuggerActive) {
    if (!isNEODebuggerActive) {
        return;
    }
    UNRECOVERABLE_IF(this->device->getDebugSurface() == nullptr);
    this->csr->makeResident(*this->device->getDebugSurface());
    if (this->device->getNEODevice()->getBindlessHeapsHelper()) {
        this->csr->makeResident(*this->device->getNEODevice()->getBindlessHeapsHelper()->getHeap(NEO::BindlessHeapsHelper::specialSsh)->getGraphicsAllocation());
    }
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
void CommandQueueHw<gfxCoreFamily>::programOneCmdListBatchBufferStart(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx) {
    if (this->dispatchCmdListBatchBufferAsPrimary) {
        programOneCmdListBatchBufferStartPrimaryBatchBuffer(commandList, commandStream, ctx);
    } else {
        programOneCmdListBatchBufferStartSecondaryBatchBuffer(commandList, commandStream, ctx);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListBatchBufferStartPrimaryBatchBuffer(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    NEO::CommandContainer &cmdListContainer = commandList->getCmdContainer();
    NEO::GraphicsAllocation *cmdListFirstCmdBuffer = cmdListContainer.getCmdBufferAllocations()[0];
    auto bbStartPatchLocation = reinterpret_cast<MI_BATCH_BUFFER_START *>(ctx.currentPatchForChainedBbStart);

    bool dynamicPreamble = ctx.childGpuAddressPositionBeforeDynamicPreamble != commandStream.getCurrentGpuAddressPosition();
    if (ctx.pipelineCmdsDispatch || dynamicPreamble || this->forceBbStartJump) {
        if (ctx.currentPatchForChainedBbStart) {
            // dynamic preamble, 2nd or later command list
            // jump from previous command list to the position before dynamic preamble
            if (this->patchingPreamble) {
                NEO::EncodeDataMemory<GfxFamily>::programBbStart(ctx.currentPatchPreambleBuffer,
                                                                 ctx.currentGpuAddressForChainedBbStart,
                                                                 ctx.childGpuAddressPositionBeforeDynamicPreamble,
                                                                 false, false, false);
            } else {
                NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(
                    bbStartPatchLocation,
                    ctx.childGpuAddressPositionBeforeDynamicPreamble,
                    false, false, false);
            }
        }
        // dynamic preamble, jump from current position, after dynamic preamble to the current command list
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStream, cmdListFirstCmdBuffer->getGpuAddress(), false, false, false);

        ctx.pipelineCmdsDispatch = false;
        this->forceBbStartJump = false;
    } else {
        if (ctx.currentPatchForChainedBbStart == nullptr) {
            // nothing to dispatch from queue, first command list will be used as submitting batch buffer to KMD or ULLS
            size_t firstCmdBufferAlignedSize = cmdListContainer.getAlignedPrimarySize();
            this->firstCmdListStream.replaceGraphicsAllocation(cmdListFirstCmdBuffer);
            this->firstCmdListStream.replaceBuffer(cmdListFirstCmdBuffer->getUnderlyingBuffer(), firstCmdBufferAlignedSize);
            this->firstCmdListStream.getSpace(firstCmdBufferAlignedSize);
            this->startingCmdBuffer = &this->firstCmdListStream;
        } else {
            // chain between command lists when no dynamic preamble required between 2nd and next command list
            if (this->patchingPreamble) {
                NEO::EncodeDataMemory<GfxFamily>::programBbStart(ctx.currentPatchPreambleBuffer,
                                                                 ctx.currentGpuAddressForChainedBbStart,
                                                                 cmdListFirstCmdBuffer->getGpuAddress(),
                                                                 false, false, false);
            } else {
                NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(
                    bbStartPatchLocation,
                    cmdListFirstCmdBuffer->getGpuAddress(),
                    false, false, false);
            }
        }
    }

    ctx.currentPatchForChainedBbStart = cmdListContainer.getEndCmdPtr();
    ctx.currentGpuAddressForChainedBbStart = cmdListContainer.getEndCmdGpuAddress();
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListBatchBufferStartSecondaryBatchBuffer(CommandList *commandList, NEO::LinearStream &commandStream, CommandListExecutionContext &ctx) {
    auto &commandContainer = commandList->getCmdContainer();

    auto &cmdBufferAllocations = commandContainer.getCmdBufferAllocations();
    auto cmdBufferCount = cmdBufferAllocations.size();
    bool isCommandListImmediate = !ctx.containsAnyRegularCmdList;

    auto &returnPoints = commandList->getReturnPoints();
    uint32_t returnPointsSize = commandList->getReturnPointsSize();
    uint32_t returnPointIdx = 0;

    for (size_t iter = 0; iter < cmdBufferCount; iter++) {
        auto allocation = cmdBufferAllocations[iter];
        uint64_t startOffset = allocation->getGpuAddress();
        if (isCommandListImmediate) {
            startOffset = ptrOffset(allocation->getGpuAddress(), commandContainer.currentLinearStreamStartOffsetRef());
        }
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStream, startOffset, true, false, false);
        if (returnPointsSize > 0) {
            bool cmdBufferHasRestarts = std::find_if(
                                            std::next(returnPoints.begin(), returnPointIdx),
                                            returnPoints.end(),
                                            [allocation](CmdListReturnPoint &retPt) {
                                                return retPt.currentCmdBuffer == allocation;
                                            }) != returnPoints.end();
            if (cmdBufferHasRestarts) {
                while (returnPointIdx < returnPointsSize && allocation == returnPoints[returnPointIdx].currentCmdBuffer) {
                    ctx.cmdListBeginState.frontEndState.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(returnPoints[returnPointIdx].configSnapshot.frontEndState);
                    programFrontEnd(ctx.scratchSpaceController->getScratchPatchAddress(),
                                    ctx.scratchSpaceController->getPerThreadScratchSpaceSizeSlot0(),
                                    commandStream,
                                    ctx.cmdListBeginState);
                    NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStream,
                                                                                         returnPoints[returnPointIdx].gpuAddress,
                                                                                         true, false, false);
                    returnPointIdx++;
                }
            }
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programLastCommandListReturnBbStart(
    NEO::LinearStream &commandStream,
    CommandListExecutionContext &ctx) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    if (this->dispatchCmdListBatchBufferAsPrimary) {
        auto finalReturnPosition = commandStream.getCurrentGpuAddressPosition();
        if (this->patchingPreamble) {
            NEO::EncodeDataMemory<GfxFamily>::programBbStart(ctx.currentPatchPreambleBuffer,
                                                             ctx.currentGpuAddressForChainedBbStart,
                                                             finalReturnPosition,
                                                             false, false, false);
        } else {
            auto bbStartCmd = reinterpret_cast<MI_BATCH_BUFFER_START *>(ctx.currentPatchForChainedBbStart);
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(bbStartCmd,
                                                                                 finalReturnPosition,
                                                                                 false, false, false);
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::collectPrintfContentsFromCommandsList(
    CommandList *commandList) {

    this->printfKernelContainer.insert(this->printfKernelContainer.end(),
                                       commandList->getPrintfKernelContainer().begin(),
                                       commandList->getPrintfKernelContainer().end());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::migrateSharedAllocationsIfRequested(
    bool isMigrationRequested,
    CommandList *commandList) {

    if (isMigrationRequested) {
        commandList->migrateSharedAllocations();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::prefetchMemoryToDeviceAssociatedWithCmdList(CommandList *commandList) {
    if (commandList->isMemoryPrefetchRequested()) {
        auto prefetchManager = this->device->getDriverHandle()->getMemoryManager()->getPrefetchManager();
        prefetchManager->migrateAllocationsToGpu(commandList->getPrefetchContext(),
                                                 *this->device->getDriverHandle()->getSvmAllocsManager(),
                                                 *this->device->getNEODevice(),
                                                 *this->csr);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::assignCsrTaskCountToFenceIfAvailable(ze_fence_handle_t hFence) {
    if (hFence) {
        Fence::fromHandle(hFence)->assignTaskCountFromCsr();
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchTaskCountPostSyncByMiFlushDw(bool isDispatchTaskCountPostSyncRequired, bool fenceRequired, NEO::LinearStream &cmdStream) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }

    if (fenceRequired) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(cmdStream, 0, NEO::FenceType::release, device->getNEODevice()->getRootDeviceEnvironment());
    }

    uint64_t postSyncAddress = this->csr->getTagAllocation()->getGpuAddress();
    TaskCountType postSyncData = this->csr->peekTaskCount() + 1;

    NEO::EncodeDummyBlitWaArgs waArgs{false, &(this->device->getNEODevice()->getRootDeviceEnvironmentRef())};
    NEO::MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = this->csr->isUsedNotifyEnableForPostSync();

    NEO::EncodeMiFlushDW<GfxFamily>::programWithWa(cmdStream, postSyncAddress, postSyncData, args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchTaskCountPostSyncRegular(
    bool isDispatchTaskCountPostSyncRequired,
    NEO::LinearStream &cmdStream) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }

    uint64_t postSyncAddress = this->csr->getTagAllocation()->getGpuAddress();
    TaskCountType postSyncData = this->csr->peekTaskCount() + 1;

    NEO::PipeControlArgs args;
    args.dcFlushEnable = this->csr->getDcFlushSupport();
    args.workloadPartitionOffset = this->partitionCount > 1;
    args.notifyEnable = this->csr->isUsedNotifyEnableForPostSync();
    args.isWalkerWithProfilingEnqueued = this->getAndClearIsWalkerWithProfilingEnqueued();
    NEO::MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        cmdStream,
        NEO::PostSyncMode::immediateData,
        postSyncAddress,
        postSyncData,
        device->getNEODevice()->getRootDeviceEnvironment(),
        args);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeCsrTagAllocationResident() {
    this->csr->makeResident(*this->csr->getTagAllocation());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::makeRayTracingBufferResident(NEO::GraphicsAllocation *rtBuffer) {
    if (rtBuffer) {
        this->csr->makeResident(*rtBuffer);
    }
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
        if (NEO::debugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
            startAddress = 0;
        }

        endingCmd = innerCommandStream.getSpace(0);
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&innerCommandStream, startAddress, false, false, false);
    } else {
        auto buffer = innerCommandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *(MI_BATCH_BUFFER_END *)buffer = GfxFamily::cmdInitBatchBufferEnd;
    }

    if (ctx.isNEODebuggerActive || NEO::debugManager.flags.EnableSWTags.get()) {
        cleanLeftoverMemory(outerCommandStream, innerCommandStream);
    } else if (this->alignedChildStreamPadding) {
        void *paddingPtr = innerCommandStream.getSpace(this->alignedChildStreamPadding);
        memset(paddingPtr, 0, this->alignedChildStreamPadding);
    }
    size_t startOffset = (this->startingCmdBuffer == &this->firstCmdListStream)
                             ? 0
                             : ptrDiff(innerCommandStream.getCpuBase(), outerCommandStream.getCpuBase());

    return submitBatchBuffer(startOffset,
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
void CommandQueueHw<gfxCoreFamily>::updateTaskCountAndPostSync(bool isDispatchTaskCountPostSyncRequired,
                                                               uint32_t numCommandLists,
                                                               ze_command_list_handle_t *commandListHandles) {

    if (!isDispatchTaskCountPostSyncRequired) {
        return;
    }
    this->taskCount = this->csr->peekTaskCount();
    this->csr->setLatestFlushedTaskCount(this->taskCount);

    this->saveTagAndTaskCountForCommandLists(numCommandLists, commandListHandles, this->csr->getTagAllocation(), this->taskCount);
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::waitForCommandQueueCompletion(CommandListExecutionContext &ctx) {
    ze_result_t ret = ZE_RESULT_SUCCESS;
    if (this->isSynchronousMode()) {
        ctx.lockCSR->unlock();
        if (const auto syncRet = this->synchronize(std::numeric_limits<uint64_t>::max()); syncRet == ZE_RESULT_ERROR_DEVICE_LOST) {
            ret = syncRet;
        }
    }

    return ret;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::handleNonParentImmediateStream(ze_fence_handle_t hFence, CommandListExecutionContext &ctx, uint32_t numCommandLists,
                                                                          ze_command_list_handle_t *phCommandLists, NEO::LinearStream *streamForDispatch) {
    this->assignCsrTaskCountToFenceIfAvailable(hFence);
    if (!this->isCopyOnlyCommandQueue) {
        this->dispatchTaskCountPostSyncRegular(ctx.isDispatchTaskCountPostSyncRequired, *streamForDispatch);
    } else {
        this->dispatchTaskCountPostSyncByMiFlushDw(ctx.isDispatchTaskCountPostSyncRequired, ctx.taskCountUpdateFenceRequired, *streamForDispatch);
    }
    auto submitResult = this->prepareAndSubmitBatchBuffer(ctx, *streamForDispatch);
    this->updateTaskCountAndPostSync(ctx.isDispatchTaskCountPostSyncRequired, numCommandLists, phCommandLists);

    this->csr->makeSurfacePackNonResident(this->csr->getResidencyAllocations(), false);

    auto retVal = this->handleSubmission(submitResult);
    this->csr->getResidencyAllocations().clear();
    this->heapContainer.clear();
    return retVal;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandQueueHw<gfxCoreFamily>::handleSubmission(
    NEO::SubmissionStatus submitRet) {
    if (submitRet != NEO::SubmissionStatus::success) {
        for (auto &gfx : this->csr->getResidencyAllocations()) {
            if (this->csr->peekLatestFlushedTaskCount() == 0) {
                gfx->releaseUsageInOsContext(this->csr->getOsContext().getContextId());
            } else {
                gfx->updateTaskCount(this->csr->peekLatestFlushedTaskCount(), this->csr->getOsContext().getContextId());
            }
        }
        return getErrorCodeForSubmissionStatus(submitRet);
    }
    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimatePipelineSelectCmdSize() {
    if (!this->pipelineSelectStateTracking) {
        bool gpgpuEnabled = csr->getPreambleSetFlag();
        return !gpgpuEnabled * NEO::PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(device->getNEODevice()->getRootDeviceEnvironment());
    }
    return 0;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimatePipelineSelectCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrState,
                                                                                           const NEO::StreamProperties &cmdListRequired,
                                                                                           const NEO::StreamProperties &cmdListFinal,
                                                                                           bool &gpgpuEnabled,
                                                                                           NEO::StreamProperties &requiredState,
                                                                                           bool &propertyDirty) {
    if (!this->pipelineSelectStateTracking) {
        return 0;
    }

    size_t estimatedSize = 0;
    bool psCurrentDirty = !gpgpuEnabled;
    if (psCurrentDirty) {
        csrState.pipelineSelect.copyPropertiesAll(cmdListRequired.pipelineSelect);
    } else {
        csrState.pipelineSelect.copyPropertiesSystolicMode(cmdListRequired.pipelineSelect);
        psCurrentDirty = csrState.pipelineSelect.isDirty();
    }

    if (psCurrentDirty) {
        estimatedSize += NEO::PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(device->getNEODevice()->getRootDeviceEnvironment());

        propertyDirty = true;
        requiredState.pipelineSelect = csrState.pipelineSelect;
    }

    if (!gpgpuEnabled) {
        csrState.pipelineSelect.copyPropertiesAll(cmdListFinal.pipelineSelect);
        gpgpuEnabled = true;
    } else {
        csrState.pipelineSelect.copyPropertiesSystolicMode(cmdListFinal.pipelineSelect);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programOneCmdListPipelineSelect(NEO::LinearStream &commandStream,
                                                                    CommandListRequiredStateChange &cmdListRequired) {
    if (!this->pipelineSelectStateTracking) {
        return;
    }

    if (cmdListRequired.flags.propertyPsDirty) {
        bool systolic = cmdListRequired.requiredState.pipelineSelect.systolicMode.value == 1;
        NEO::PipelineSelectArgs args = {
            systolic,
            false,
            cmdListRequired.commandList->getSystolicModeSupport()};

        NEO::PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, args, device->getNEODevice()->getRootDeviceEnvironment());
        csr->setPreambleSetFlag(true);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateScmCmdSizeForMultipleCommandLists(NEO::StreamProperties &csrState,
                                                                                bool &scmStateDirty,
                                                                                const NEO::StreamProperties &cmdListRequired,
                                                                                const NEO::StreamProperties &cmdListFinal,
                                                                                NEO::StreamProperties &requiredState,
                                                                                bool &propertyDirty) {
    if (!this->stateComputeModeTracking) {
        return 0;
    }

    size_t estimatedSize = 0;
    bool scmCurrentDirty = scmStateDirty;
    if (scmStateDirty) {
        csrState.stateComputeMode.copyPropertiesAll(cmdListRequired.stateComputeMode);
    } else {
        csrState.stateComputeMode.copyPropertiesGrfNumberThreadArbitration(cmdListRequired.stateComputeMode);
        scmCurrentDirty = csrState.stateComputeMode.isDirty();
    }

    if (scmCurrentDirty) {
        bool isRcs = this->getCsr()->isRcs();
        estimatedSize = NEO::EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(device->getNEODevice()->getRootDeviceEnvironment(), false, isRcs);

        propertyDirty = true;
        requiredState.stateComputeMode = csrState.stateComputeMode;
        requiredState.pipelineSelect = csrState.pipelineSelect;
    }

    if (scmStateDirty) {
        csrState.stateComputeMode.copyPropertiesAll(cmdListFinal.stateComputeMode);
        scmStateDirty = false;
    } else {
        csrState.stateComputeMode.copyPropertiesGrfNumberThreadArbitration(cmdListFinal.stateComputeMode);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programRequiredStateComputeModeForCommandList(NEO::LinearStream &commandStream,
                                                                                  CommandListRequiredStateChange &cmdListRequired) {
    if (!this->stateComputeModeTracking) {
        return;
    }

    if (cmdListRequired.flags.propertyScmDirty) {
        NEO::PipelineSelectArgs pipelineSelectArgs = {
            cmdListRequired.requiredState.pipelineSelect.systolicMode.value == 1,
            false,
            cmdListRequired.commandList->getSystolicModeSupport()};

        NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(commandStream, cmdListRequired.requiredState.stateComputeMode, pipelineSelectArgs,
                                                                                        false, device->getNEODevice()->getRootDeviceEnvironment(), this->csr->isRcs(),
                                                                                        this->csr->getDcFlushSupport());
        this->csr->setStateComputeModeDirty(false);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programRequiredStateBaseAddressForCommandList(CommandListExecutionContext &ctx,
                                                                                  NEO::LinearStream &commandStream,
                                                                                  CommandListRequiredStateChange &cmdListRequired) {

    if (!this->stateBaseAddressTracking) {
        return;
    }

    if (cmdListRequired.flags.propertySbaDirty) {
        bool indirectHeapInLocalMemory = cmdListRequired.commandList->getCmdContainer().isIndirectHeapInLocalMemory();
        programStateBaseAddress(ctx.scratchGsba,
                                indirectHeapInLocalMemory,
                                commandStream,
                                ctx.cachedMOCSAllowed,
                                &cmdListRequired.requiredState);

        ctx.gsbaStateDirty = false;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateBaseAddressState(CommandList *lastCommandList) {
    auto csrHw = static_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(csr);
    auto &streamProperties = this->csr->getStreamProperties();

    const auto bindlessHeapsHelper = device->getNEODevice()->getBindlessHeapsHelper();
    const bool useGlobalHeaps = bindlessHeapsHelper != nullptr;

    auto &commandContainer = lastCommandList->getCmdContainer();

    if (lastCommandList->getCmdListHeapAddressModel() == NEO::HeapAddressModel::globalStateless) {
        auto globalStateless = csr->getGlobalStatelessHeap();
        csrHw->getSshState().updateAndCheck(globalStateless);
        streamProperties.stateBaseAddress.setPropertiesSurfaceState(globalStateless->getHeapGpuBase(), globalStateless->getHeapSizeInPages());
    } else {
        auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::dynamicState);
        if (dsh != nullptr) {
            auto stateBaseAddress = NEO::getStateBaseAddress(*dsh, useGlobalHeaps);
            auto stateSize = NEO::getStateSize(*dsh, useGlobalHeaps);

            csrHw->getDshState().updateAndCheck(dsh, stateBaseAddress, stateSize);
            streamProperties.stateBaseAddress.setPropertiesDynamicState(stateBaseAddress, stateSize);
        }

        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (ssh != nullptr) {
            auto stateBaseAddress = NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps);
            auto stateSize = NEO::getStateSizeForSsh(*ssh, useGlobalHeaps);

            csrHw->getSshState().updateAndCheck(ssh, stateBaseAddress, stateSize);
            streamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(stateBaseAddress,
                                                                                    stateSize,
                                                                                    stateBaseAddress,
                                                                                    stateSize);
        }
    }

    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::indirectObject);
    csrHw->getIohState().updateAndCheck(ioh);
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::updateDebugSurfaceState(CommandListExecutionContext &ctx) {
    if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        if (ctx.isNEODebuggerActive && ctx.gsbaStateDirty) {
            auto globalStatelessHeap = this->csr->getGlobalStatelessHeap();

            auto surfaceStateSpace = this->device->getNEODevice()->getDebugger()->getDebugSurfaceReservedSurfaceState(*globalStatelessHeap);
            auto surfaceState = GfxFamily::cmdInitRenderSurfaceState;

            NEO::EncodeSurfaceStateArgs args;
            args.outMemory = &surfaceState;
            args.graphicsAddress = this->device->getDebugSurface()->getGpuAddress();
            args.size = this->device->getDebugSurface()->getUnderlyingBufferSize();
            args.mocs = this->device->getMOCS(false, false);
            args.numAvailableDevices = this->device->getNEODevice()->getNumGenericSubDevices();
            args.allocation = this->device->getDebugSurface();
            args.gmmHelper = this->device->getNEODevice()->getGmmHelper();
            args.areMultipleSubDevicesInContext = false;
            args.isDebuggerActive = true;
            NEO::EncodeSurfaceState<GfxFamily>::encodeBuffer(args);
            *reinterpret_cast<typename GfxFamily::RENDER_SURFACE_STATE *>(surfaceStateSpace) = surfaceState;
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSizeForMultipleCommandLists(bool &baseAddressStateDirty,
                                                                                             NEO::HeapAddressModel commandListHeapAddressModel,
                                                                                             NEO::StreamProperties &csrState,
                                                                                             const NEO::StreamProperties &cmdListRequired,
                                                                                             const NEO::StreamProperties &cmdListFinal,
                                                                                             NEO::StreamProperties &requiredState,
                                                                                             bool &propertyDirty) {
    if (!this->stateBaseAddressTracking) {
        return 0;
    }

    size_t estimatedSize = 0;

    if (commandListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
        estimatedSize = estimateStateBaseAddressCmdSizeForGlobalStatelessCommandList(baseAddressStateDirty, csrState, cmdListRequired, cmdListFinal, requiredState, propertyDirty);
    } else {
        estimatedSize = estimateStateBaseAddressCmdSizeForPrivateHeapCommandList(baseAddressStateDirty, csrState, cmdListRequired, cmdListFinal, requiredState, propertyDirty);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSizeForGlobalStatelessCommandList(bool &baseAddressStateDirty,
                                                                                                   NEO::StreamProperties &csrState,
                                                                                                   const NEO::StreamProperties &cmdListRequired,
                                                                                                   const NEO::StreamProperties &cmdListFinal,
                                                                                                   NEO::StreamProperties &requiredState,
                                                                                                   bool &propertyDirty) {
    auto globalStatelessHeap = this->csr->getGlobalStatelessHeap();

    size_t estimatedSize = 0;

    if (baseAddressStateDirty) {
        csrState.stateBaseAddress.copyPropertiesAll(cmdListRequired.stateBaseAddress);
    } else {
        csrState.stateBaseAddress.copyPropertiesStatelessMocs(cmdListRequired.stateBaseAddress);
    }
    csrState.stateBaseAddress.setPropertiesSurfaceState(globalStatelessHeap->getHeapGpuBase(), globalStatelessHeap->getHeapSizeInPages());

    if (baseAddressStateDirty || csrState.stateBaseAddress.isDirty()) {
        bool useBtiCommand = csrState.stateBaseAddress.bindingTablePoolBaseAddress.value != NEO::StreamProperty64::initValue;
        estimatedSize = estimateStateBaseAddressCmdDispatchSize(useBtiCommand);

        propertyDirty = true;
        requiredState.stateBaseAddress = csrState.stateBaseAddress;
    }

    if (baseAddressStateDirty) {
        csrState.stateBaseAddress.copyPropertiesAll(cmdListFinal.stateBaseAddress);
        baseAddressStateDirty = false;
    } else {
        csrState.stateBaseAddress.copyPropertiesStatelessMocs(cmdListFinal.stateBaseAddress);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressCmdSizeForPrivateHeapCommandList(bool &baseAddressStateDirty,
                                                                                               NEO::StreamProperties &csrState,
                                                                                               const NEO::StreamProperties &cmdListRequired,
                                                                                               const NEO::StreamProperties &cmdListFinal,
                                                                                               NEO::StreamProperties &requiredState,
                                                                                               bool &propertyDirty) {
    size_t estimatedSize = 0;

    csrState.stateBaseAddress.copyPropertiesAll(cmdListRequired.stateBaseAddress);
    if (baseAddressStateDirty || csrState.stateBaseAddress.isDirty()) {
        bool useBtiCommand = csrState.stateBaseAddress.bindingTablePoolBaseAddress.value != NEO::StreamProperty64::initValue;
        estimatedSize = estimateStateBaseAddressCmdDispatchSize(useBtiCommand);

        baseAddressStateDirty = false;
        propertyDirty = true;
        requiredState.stateBaseAddress = csrState.stateBaseAddress;
    }
    csrState.stateBaseAddress.copyPropertiesAll(cmdListFinal.stateBaseAddress);

    return estimatedSize;
}
template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateBaseAddressDebugTracking() {
    size_t size = 0;
    if (NEO::Debugger::isDebugEnabled(this->internalUsage) && this->device->getL0Debugger() != nullptr) {
        const size_t trackedAddressesCount = 6;
        size = this->device->getL0Debugger()->getSbaTrackingCommandsSize(trackedAddressesCount);
    }
    return size;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::patchCommands(CommandList &commandList, CommandListExecutionContext &ctx) {
    bool patchNewScratchController = false;
    uint64_t scratchAddress = ctx.scratchSpaceController->getScratchPatchAddress();

    if (this->heaplessModeEnabled) {
        if (this->cmdListHeapAddressModel == NEO::HeapAddressModel::globalStateless) {
            scratchAddress += ctx.globalStatelessAllocation->getGpuAddress();
        }

        if (commandList.getCommandListUsedScratchController() != ctx.scratchSpaceController) {
            patchNewScratchController = true;
        }
    }

    patchCommands(commandList, scratchAddress, patchNewScratchController, &ctx.currentPatchPreambleBuffer);

    if (patchNewScratchController) {
        commandList.setCommandListUsedScratchController(ctx.scratchSpaceController);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::prepareInOrderCommandList(CommandListImp *commandList, CommandListExecutionContext &ctx) {
    if (commandList->inOrderCmdsPatchingEnabled()) {
        UNRECOVERABLE_IF(this->patchingPreamble);
        commandList->addRegularCmdListSubmissionCounter();
        commandList->patchInOrderCmds();
    } else {
        if (this->patchingPreamble) {
            ctx.totalNoopSpaceForPatchPreamble += commandList->getInOrderExecDeviceRequiredSize();
            ctx.totalNoopSpaceForPatchPreamble += commandList->getInOrderExecHostRequiredSize();
        } else {
            commandList->clearInOrderExecCounterAllocation();
        }
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programRequiredCacheFlushes(CommandListExecutionContext &ctx,
                                                                NEO::LinearStream &commandStream) {
    if (ctx.instructionCacheFlushRequired) {
        NEO::MemorySynchronizationCommands<GfxFamily>::addInstructionCacheFlush(commandStream);
        this->csr->setInstructionCacheFlushed();
    }

    if (ctx.stateCacheFlushRequired) {
        auto neoDevice = this->device->getNEODevice();
        NEO::MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(commandStream, neoDevice->getRootDeviceEnvironment());
        neoDevice->getBindlessHeapsHelper()->clearStateDirtyForContext(this->csr->getOsContext().getContextId());
    }
}

} // namespace L0
