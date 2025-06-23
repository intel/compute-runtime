/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"
#include "shared/source/utilities/wait_util.h"

#include "command_stream_receiver_hw_ext.inl"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiverHw<GfxFamily>::~CommandStreamReceiverHw() {
    this->unregisterDirectSubmissionFromController();
    if (completionFenceValuePointer) {
        completionFenceValue = *completionFenceValuePointer;
        completionFenceValuePointer = &completionFenceValue;
    }
}

template <typename GfxFamily>
CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                                                            uint32_t rootDeviceIndex,
                                                            const DeviceBitfield deviceBitfield)
    : CommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {

    const auto &hwInfo = peekHwInfo();
    auto &gfxCoreHelper = getGfxCoreHelper();
    localMemoryEnabled = gfxCoreHelper.getEnableLocalMemory(hwInfo);

    resetKmdNotifyHelper(new KmdNotifyHelper(&hwInfo.capabilityTable.kmdNotifyProperties));

    if (debugManager.flags.FlattenBatchBufferForAUBDump.get() || debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        flatBatchBufferHelper.reset(new FlatBatchBufferHelperHw<GfxFamily>(executionEnvironment));
    }
    defaultSshSize = HeapSize::getDefaultHeapSize(EncodeStates<GfxFamily>::getSshHeapSize());
    this->use4GbHeaps = are4GbHeapsAvailable();

    timestampPacketWriteEnabled = gfxCoreHelper.timestampPacketWriteSupported();
    if (debugManager.flags.EnableTimestampPacket.get() != -1) {
        timestampPacketWriteEnabled = !!debugManager.flags.EnableTimestampPacket.get();
    }

    createScratchSpaceController();
    configurePostSyncWriteOffset();

    this->dcFlushSupport = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    this->dshSupported = hwInfo.capabilityTable.supportsImages;
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    return SubmissionStatus::success;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addBatchBufferEnd(LinearStream &commandStream, void **patchLocation) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto pCmd = commandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *pCmd = GfxFamily::cmdInitBatchBufferEnd;
    if (patchLocation) {
        *patchLocation = pCmd;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEndingCmd(LinearStream &commandStream, void **patchLocation, bool directSubmissionEnabled,
                                                                 bool hasRelaxedOrderingDependencies, bool isBcs) {
    if (directSubmissionEnabled) {
        uint64_t startAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed();
        if (debugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
            startAddress = 0;
        }

        bool relaxedOrderingEnabled = false;
        if (isBlitterDirectSubmissionEnabled() && EngineHelpers::isBcs(this->osContext->getEngineType())) {
            relaxedOrderingEnabled = this->blitterDirectSubmission->isRelaxedOrderingEnabled();
        } else if (isDirectSubmissionEnabled()) {
            relaxedOrderingEnabled = this->directSubmission->isRelaxedOrderingEnabled();
        }

        bool indirect = false;
        if (relaxedOrderingEnabled && hasRelaxedOrderingDependencies) {
            NEO::EncodeSetMMIO<GfxFamily>::encodeREG(commandStream, RegisterOffsets::csGprR0, RegisterOffsets::csGprR3, isBcs);
            NEO::EncodeSetMMIO<GfxFamily>::encodeREG(commandStream, RegisterOffsets::csGprR0 + 4, RegisterOffsets::csGprR3 + 4, isBcs);

            indirect = true;
        }

        *patchLocation = commandStream.getSpace(0);

        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStream, startAddress, false, indirect, false);

    } else {
        this->addBatchBufferEnd(commandStream, patchLocation);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary) {
    MI_BATCH_BUFFER_START cmd = GfxFamily::cmdInitBatchBufferStart;

    cmd.setBatchBufferStartAddress(startAddress);
    cmd.setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    if (secondary) {
        cmd.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    }
    if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        flatBatchBufferHelper->registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commandBufferMemory), startAddress);
    }
    *commandBufferMemory = cmd;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdSizeForPreamble(Device &device) const {
    size_t size = 0;

    if (mediaVfeStateDirty) {
        size += PreambleHelper<GfxFamily>::getVFECommandsSize();
    }
    if (!this->isPreambleSent) {
        size += PreambleHelper<GfxFamily>::getAdditionalCommandsSize(device);
    }
    if (!this->isPreambleSent) {
        if (debugManager.flags.ForceSemaphoreDelayBetweenWaits.get() > -1) {
            size += PreambleHelper<GfxFamily>::getSemaphoreDelayCommandSize();
        }
    }
    return size;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programHardwareContext(LinearStream &cmdStream) {
    programEnginePrologue(cmdStream);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdsSizeForHardwareContext() const {
    return getCmdSizeForPrologue();
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::addPipeControlFlushTaskIfNeeded(LinearStream &commandStreamCSR, TaskCountType taskLevel) {

    if (this->requiresInstructionCacheFlush) {
        MemorySynchronizationCommands<GfxFamily>::addInstructionCacheFlush(commandStreamCSR);
        this->requiresInstructionCacheFlush = false;
    }

    // Add a Pipe Control if we have a dependency on a previous walker to avoid concurrency issues.
    if (taskLevel > this->taskLevel) {
        const auto programPipeControl = !timestampPacketWriteEnabled;
        if (programPipeControl) {
            PipeControlArgs args;
            MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);
        }
        this->taskLevel = taskLevel;
        DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskCount", peekTaskCount());
    }

    if (debugManager.flags.ForcePipeControlPriorToWalker.get()) {
        forcePipeControl(commandStreamCSR);
    }
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                                                 const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) {
    UNRECOVERABLE_IF(this->dispatchMode != DispatchMode::immediateDispatch);

    uint64_t taskStartAddress = commandStreamTask.getGpuBase() + commandStreamTaskStart;

    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&(this->peekRootDeviceEnvironment()))};

    LinearStream &epilogueCommandStream = dispatchBcsFlags.optionalEpilogueCmdStream != nullptr ? *dispatchBcsFlags.optionalEpilogueCmdStream : commandStreamTask;

    if (dispatchBcsFlags.flushTaskCount) {
        uint64_t postSyncAddress = getTagAllocation()->getGpuAddress();
        TaskCountType postSyncData = peekTaskCount() + 1;
        NEO::MiFlushArgs args{waArgs};
        args.commandWithPostSync = true;
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        args.tlbFlush |= (debugManager.flags.ForceTlbFlushWithTaskCountAfterCopy.get() == 1);

        NEO::EncodeMiFlushDW<GfxFamily>::programWithWa(epilogueCommandStream, postSyncAddress, postSyncData, args);
    }

    auto &commandStreamCSR = getCS(getRequiredCmdStreamSizeAligned(dispatchBcsFlags));
    size_t commandStreamStartCSR = commandStreamCSR.getUsed();

    if (dispatchBcsFlags.dispatchOperation != AppendOperations::cmdList) {
        programHardwareContext(commandStreamCSR);
    }

    if (debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
        MiFlushArgs tlbFlushArgs{waArgs};
        tlbFlushArgs.commandWithPostSync = true;
        tlbFlushArgs.tlbFlush = true;

        EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, this->getGlobalFenceAllocation()->getGpuAddress(), 0, tlbFlushArgs);
    }

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    makeResident(*getTagAllocation());

    makeResident(*commandStreamTask.getGraphicsAllocation());
    if (dispatchBcsFlags.optionalEpilogueCmdStream != nullptr) {
        makeResident(*dispatchBcsFlags.optionalEpilogueCmdStream->getGraphicsAllocation());
    }

    bool submitCSR = (commandStreamStartCSR != commandStreamCSR.getUsed());
    void *bbEndLocation = nullptr;

    programEndingCmd(epilogueCommandStream, &bbEndLocation, isBlitterDirectSubmissionEnabled(), dispatchBcsFlags.hasRelaxedOrderingDependencies, true);
    EncodeNoop<GfxFamily>::alignToCacheLine(epilogueCommandStream);

    if (submitCSR) {
        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(commandStreamCSR.getSpace(sizeof(MI_BATCH_BUFFER_START)));
        addBatchBufferStart(bbStart, taskStartAddress, false);
        EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);

        this->makeResident(*commandStreamCSR.getGraphicsAllocation());
    }

    size_t startOffset = submitCSR ? commandStreamStartCSR : commandStreamTaskStart;
    auto &streamToSubmit = submitCSR ? commandStreamCSR : commandStreamTask;

    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, 0, taskStartAddress, nullptr,
                            false, getThrottleFromPowerSavingUint(this->getUmdPowerHintValue()), NEO::QueueSliceCount::defaultSliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, bbEndLocation, this->getNumClients(), (submitCSR || dispatchBcsFlags.hasStallingCmds || dispatchBcsFlags.flushTaskCount),
                            dispatchBcsFlags.hasRelaxedOrderingDependencies, dispatchBcsFlags.flushTaskCount, false};
    batchBuffer.disableFlatRingBuffer = dispatchBcsFlags.dispatchOperation == AppendOperations::cmdList;
    updateStreamTaskCount(streamToSubmit, taskCount + 1);
    this->latestSentTaskCount = taskCount + 1;

    auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
    if (submissionStatus != SubmissionStatus::success) {
        updateStreamTaskCount(streamToSubmit, taskCount);
        CompletionStamp completionStamp = {CompletionStamp::getTaskCountFromSubmissionStatusError(submissionStatus)};
        return completionStamp;
    }

    if (dispatchBcsFlags.flushTaskCount) {
        this->latestFlushedTaskCount = this->taskCount + 1;
    }

    ++taskCount;

    CompletionStamp completionStamp = {taskCount, taskLevel, flushStamp->peekStamp()};

    return completionStamp;
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushImmediateTask(
    LinearStream &immediateCommandStream,
    size_t immediateCommandStreamStart,
    ImmediateDispatchFlags &dispatchFlags,
    Device &device) {

    ImmediateFlushData flushData;
    if (dispatchFlags.dispatchOperation != AppendOperations::cmdList) {
        flushData.pipelineSelectFullConfigurationNeeded = !getPreambleSetFlag();
        flushData.frontEndFullConfigurationNeeded = getMediaVFEStateDirty();
        flushData.stateComputeModeFullConfigurationNeeded = getStateComputeModeDirty();
        flushData.stateBaseAddressFullConfigurationNeeded = getGSBAStateDirty();

        if (!this->heaplessModeEnabled && dispatchFlags.sshCpuBase != nullptr && (this->requiredScratchSlot0Size > 0 || this->requiredScratchSlot1Size > 0)) {
            bool checkFeStateDirty = false;
            bool checkSbaStateDirty = false;
            scratchSpaceController->setRequiredScratchSpace(dispatchFlags.sshCpuBase,
                                                            0u,
                                                            this->requiredScratchSlot0Size,
                                                            this->requiredScratchSlot1Size,
                                                            *this->osContext,
                                                            checkSbaStateDirty,
                                                            checkFeStateDirty);
            flushData.frontEndFullConfigurationNeeded |= checkFeStateDirty;
            flushData.stateBaseAddressFullConfigurationNeeded |= checkSbaStateDirty;

            if (scratchSpaceController->getScratchSpaceSlot0Allocation()) {
                makeResident(*scratchSpaceController->getScratchSpaceSlot0Allocation());
            }
            if (scratchSpaceController->getScratchSpaceSlot1Allocation()) {
                makeResident(*scratchSpaceController->getScratchSpaceSlot1Allocation());
            }
        }

        handleImmediateFlushPipelineSelectState(dispatchFlags, flushData);
        handleImmediateFlushFrontEndState(dispatchFlags, flushData);
        handleImmediateFlushStateComputeModeState(dispatchFlags, flushData);
        handleImmediateFlushStateBaseAddressState(dispatchFlags, flushData, device);
        handleImmediateFlushOneTimeContextInitState(dispatchFlags, flushData, device);

        flushData.stateCacheFlushRequired = device.getBindlessHeapsHelper() ? device.getBindlessHeapsHelper()->getStateDirtyForContext(getOsContext().getContextId()) : false;
        if (flushData.stateCacheFlushRequired) {
            flushData.estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        }

        if (requiresInstructionCacheFlush) {
            flushData.estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForInstructionCacheFlush();
        }
    }

    // this must be the last call after all estimate size operations
    handleImmediateFlushJumpToImmediate(flushData);

    auto &csrCommandStream = getCS(flushData.estimatedSize);
    flushData.csrStartOffset = csrCommandStream.getUsed();

    if (dispatchFlags.dispatchOperation != AppendOperations::cmdList) {
        if (flushData.stateCacheFlushRequired) {
            device.getBindlessHeapsHelper()->clearStateDirtyForContext(getOsContext().getContextId());
            MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(csrCommandStream, device.getRootDeviceEnvironment());
        }

        if (requiresInstructionCacheFlush) {
            MemorySynchronizationCommands<GfxFamily>::addInstructionCacheFlush(csrCommandStream);
            requiresInstructionCacheFlush = false;
        }

        dispatchImmediateFlushPipelineSelectCommand(flushData, csrCommandStream);
        dispatchImmediateFlushFrontEndCommand(flushData, device, csrCommandStream);
        dispatchImmediateFlushStateComputeModeCommand(flushData, csrCommandStream);
        dispatchImmediateFlushStateBaseAddressCommand(flushData, csrCommandStream, device);
        dispatchImmediateFlushOneTimeContextInitCommand(flushData, csrCommandStream, device);
    }

    dispatchImmediateFlushJumpToImmediateCommand(immediateCommandStream, immediateCommandStreamStart, flushData, csrCommandStream);

    dispatchImmediateFlushClientBufferCommands(dispatchFlags, immediateCommandStream, flushData);

    handleImmediateFlushAllocationsResidency(device,
                                             immediateCommandStream,
                                             flushData,
                                             csrCommandStream);

    return handleImmediateFlushSendBatchBuffer(immediateCommandStream,
                                               immediateCommandStreamStart,
                                               dispatchFlags,
                                               flushData,
                                               csrCommandStream);
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTask(
    LinearStream &commandStreamTask,
    size_t commandStreamStartTask,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    TaskCountType taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {

    if (this->getHeaplessStateInitEnabled()) {
        return flushTaskHeapless(commandStreamTask, commandStreamStartTask, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    } else {
        return flushTaskHeapful(commandStreamTask, commandStreamStartTask, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTaskHeapful(
    LinearStream &commandStreamTask,
    size_t commandStreamStartTask,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    TaskCountType taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {

    DEBUG_BREAK_IF(&commandStreamTask == &commandStream);
    DEBUG_BREAK_IF(!(dispatchFlags.preemptionMode == PreemptionMode::Disabled ? device.getPreemptionMode() == PreemptionMode::Disabled : true));
    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskLevel", taskLevel);

    auto levelClosed = false;
    bool implicitFlush = dispatchFlags.implicitFlush || dispatchFlags.blocking || debugManager.flags.ForceImplicitFlush.get();
    void *currentPipeControlForNooping = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    PipeControlArgs args;

    if (debugManager.flags.ForceCsrFlushing.get()) {
        flushBatchedSubmissions();
    }

    if (detectInitProgrammingFlagsRequired(dispatchFlags)) {
        initProgrammingFlags();
    }

    const auto &hwInfo = peekHwInfo();

    bool hasStallingCmdsOnTaskStream = false;

    if (dispatchFlags.blocking || dispatchFlags.dcFlush || dispatchFlags.guardCommandBufferWithPipeControl || this->heapStorageRequiresRecyclingTag) {
        LinearStream &epilogueCommandStream = dispatchFlags.optionalEpilogueCmdStream != nullptr ? *dispatchFlags.optionalEpilogueCmdStream : commandStreamTask;
        processBarrierWithPostSync(epilogueCommandStream, dispatchFlags, levelClosed, currentPipeControlForNooping,
                                   epiloguePipeControlLocation, hasStallingCmdsOnTaskStream, args);
    }
    this->latestSentTaskCount = taskCount + 1;

    if (debugManager.flags.ForceSLML3Config.get()) {
        dispatchFlags.useSLM = true;
    }

    auto newL3Config = PreambleHelper<GfxFamily>::getL3Config(hwInfo, dispatchFlags.useSLM);

    dispatchFlags.pipelineSelectArgs.systolicPipelineSelectSupport = this->pipelineSupportFlags.systolicMode;
    handlePipelineSelectStateTransition(dispatchFlags);

    this->streamProperties.stateComputeMode.setPropertiesAll(false, dispatchFlags.numGrfRequired,
                                                             dispatchFlags.threadArbitrationPolicy, device.getPreemptionMode());

    csrSizeRequestFlags.l3ConfigChanged = this->lastSentL3Config != newL3Config;
    csrSizeRequestFlags.preemptionRequestChanged = this->lastPreemptionMode != dispatchFlags.preemptionMode;

    csrSizeRequestFlags.activePartitionsChanged = isProgramActivePartitionConfigRequired();
    bool stateBaseAddressDirty = false;

    bool checkVfeStateDirty = false;
    if (heaplessModeEnabled == false) {
        if (ssh && (requiredScratchSlot0Size || requiredScratchSlot1Size)) {
            scratchSpaceController->setRequiredScratchSpace(ssh->getCpuBase(),
                                                            0u,
                                                            requiredScratchSlot0Size,
                                                            requiredScratchSlot1Size,
                                                            *this->osContext,
                                                            stateBaseAddressDirty,
                                                            checkVfeStateDirty);
            if (checkVfeStateDirty) {
                setMediaVFEStateDirty(true);
            }
            if (scratchSpaceController->getScratchSpaceSlot0Allocation()) {
                makeResident(*scratchSpaceController->getScratchSpaceSlot0Allocation());
            }
            if (scratchSpaceController->getScratchSpaceSlot1Allocation()) {
                makeResident(*scratchSpaceController->getScratchSpaceSlot1Allocation());
            }
        }
    }

    if (dispatchFlags.usePerDssBackedBuffer) {
        if (!perDssBackedBuffer) {
            createPerDssBackedBuffer(device);
        }
        makeResident(*perDssBackedBuffer);
    }

    handleFrontEndStateTransition(dispatchFlags);

    auto estimatedSize = getRequiredCmdStreamSizeAligned(dispatchFlags, device);

    bool stateCacheFlushRequired = device.getBindlessHeapsHelper() ? device.getBindlessHeapsHelper()->getStateDirtyForContext(getOsContext().getContextId()) : false;
    if (stateCacheFlushRequired) {
        estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }
    auto &commandStreamCSR = this->getCS(estimatedSize);
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies, false, EngineHelpers::isBcs(this->osContext->getEngineType()));
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);

    programActivePartitionConfigFlushTask(commandStreamCSR);
    programEngineModeCommands(commandStreamCSR, dispatchFlags);

    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = pageTableManager->initPageTableManagerRegisters(this);
    }

    programHardwareContext(commandStreamCSR);
    programPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs);
    programComputeMode(commandStreamCSR, dispatchFlags, hwInfo);
    programL3(commandStreamCSR, newL3Config, EngineHelpers::isBcs(this->osContext->getEngineType()));
    programPreamble(commandStreamCSR, device, newL3Config);
    programMediaSampler(commandStreamCSR, dispatchFlags);
    addPipeControlBefore3dState(commandStreamCSR, dispatchFlags);
    programPerDssBackedBuffer(commandStreamCSR, device, dispatchFlags);
    if (isRayTracingStateProgramingNeeded(device)) {
        dispatchRayTracingStateCommand(commandStreamCSR, device);
    }

    if (this->heaplessModeEnabled == false) {
        programVFEState(commandStreamCSR, dispatchFlags, device.getDeviceInfo().maxFrontEndThreads);
    }

    programPreemption(commandStreamCSR, dispatchFlags);

    if (dispatchFlags.isStallingCommandsOnNextFlushRequired) {
        programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, dispatchFlags.isDcFlushRequiredOnStallingCommandsOnNextFlush);
    }

    programStateBaseAddress(dsh, ioh, ssh, dispatchFlags, device, commandStreamCSR, stateBaseAddressDirty);
    addPipeControlBeforeStateSip(commandStreamCSR, device);
    programStateSip(commandStreamCSR, device);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskLevel", (uint32_t)this->taskLevel);

    bool samplerCacheFlushBetweenRedescribedSurfaceReadsRequired = hwInfo.workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads;
    if (samplerCacheFlushBetweenRedescribedSurfaceReadsRequired) {
        programSamplerCacheFlushBetweenRedescribedSurfaceReads(commandStreamCSR);
    }

    if (stateCacheFlushRequired) {
        device.getBindlessHeapsHelper()->clearStateDirtyForContext(getOsContext().getContextId());
        MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(commandStreamCSR, device.getRootDeviceEnvironment());
    }

    addPipeControlFlushTaskIfNeeded(commandStreamCSR, taskLevel);

    this->makeResident(*tagAllocation);

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    if (getPreemptionAllocation()) {
        makeResident(*getPreemptionAllocation());
    }

    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (device.isStateSipRequired()) {
        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, this->osContext).getSipAllocation();
        makeResident(*sipAllocation);
    }

    if (debuggingEnabled && debugSurface) {
        makeResident(*debugSurface);
    }

    if (getWorkPartitionAllocation()) {
        makeResident(*getWorkPartitionAllocation());
    }

    if (dispatchFlags.optionalEpilogueCmdStream != nullptr) {
        makeResident(*dispatchFlags.optionalEpilogueCmdStream->getGraphicsAllocation());
    }

    auto rtBuffer = device.getRTMemoryBackedBuffer();
    if (rtBuffer) {
        makeResident(*rtBuffer);
    }

    bool submitTask = commandStreamStartTask != commandStreamTask.getUsed();
    bool submitCSR = commandStreamStartCSR != commandStreamCSR.getUsed();

    auto batchBuffer = prepareBatchBufferForSubmission(commandStreamTask, commandStreamStartTask, commandStreamCSR, commandStreamStartCSR,
                                                       dispatchFlags, device, submitTask, submitCSR, hasStallingCmdsOnTaskStream);

    auto completionStamp = handleFlushTaskSubmission(std::move(batchBuffer), dispatchFlags, device, currentPipeControlForNooping, epiloguePipeControlLocation,
                                                     args, submitTask, submitCSR, hasStallingCmdsOnTaskStream, levelClosed, implicitFlush);

    return completionStamp;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::forcePipeControl(NEO::LinearStream &commandStreamCSR) {
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);

    args.csStallOnly = false;
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags, const HardwareInfo &hwInfo) {
    if (this->streamProperties.stateComputeMode.isDirty()) {
        EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(
            stream, this->streamProperties.stateComputeMode, dispatchFlags.pipelineSelectArgs,
            hasSharedHandles(), this->peekRootDeviceEnvironment(), isRcs(), this->dcFlushSupport);
        this->setStateComputeModeDirty(false);
        this->streamProperties.stateComputeMode.clearIsDirty();
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingCommandsForBarrier(LinearStream &cmdStream, TimestampPacketContainer *barrierTimestampPacketNodes, const bool isDcFlushRequired) {
    if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() != 0) {
        programStallingPostSyncCommandsForBarrier(cmdStream, *barrierTimestampPacketNodes->peekNodes()[0], isDcFlushRequired);
        barrierTimestampPacketNodes->makeResident(*this);
    } else {
        programStallingNoPostSyncCommandsForBarrier(cmdStream);
    }
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::flushBatchedSubmissions() {
    if (this->dispatchMode == DispatchMode::immediateDispatch) {
        return true;
    }
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    std::unique_lock<MutexType> lockGuard(ownershipMutex);
    bool submitResult = true;

    auto &commandBufferList = this->submissionAggregator->peekCmdBufferList();
    if (!commandBufferList.peekIsEmpty()) {
        const auto totalMemoryBudget = static_cast<size_t>(commandBufferList.peekHead()->device.getDeviceInfo().globalMemSize / 2);

        ResidencyContainer surfacesForSubmit;
        ResourcePackage resourcePackage;
        void *currentPipeControlForNooping = nullptr;
        void *epiloguePipeControlLocation = nullptr;

        while (!commandBufferList.peekIsEmpty()) {
            size_t totalUsedSize = 0u;
            this->submissionAggregator->aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, osContext->getContextId());
            auto primaryCmdBuffer = commandBufferList.removeFrontOne();
            auto nextCommandBuffer = commandBufferList.peekHead();
            auto currentBBendLocation = primaryCmdBuffer->batchBufferEndLocation;
            auto lastTaskCount = primaryCmdBuffer->taskCount;
            auto lastPipeControlArgs = primaryCmdBuffer->epiloguePipeControlArgs;

            auto pipeControlLocationSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);

            FlushStampUpdateHelper flushStampUpdateHelper;
            flushStampUpdateHelper.insert(primaryCmdBuffer->flushStamp->getStampReference());

            currentPipeControlForNooping = primaryCmdBuffer->pipeControlThatMayBeErasedLocation;
            epiloguePipeControlLocation = primaryCmdBuffer->epiloguePipeControlLocation;

            if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                flatBatchBufferHelper->registerCommandChunk(primaryCmdBuffer->batchBuffer, sizeof(MI_BATCH_BUFFER_START));
            }

            while (nextCommandBuffer && nextCommandBuffer->inspectionId == primaryCmdBuffer->inspectionId) {

                // noop pipe control
                if (currentPipeControlForNooping) {
                    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
                        flatBatchBufferHelper->removePipeControlData(pipeControlLocationSize, currentPipeControlForNooping, peekRootDeviceEnvironment());
                    }
                    memset(currentPipeControlForNooping, 0, pipeControlLocationSize);
                }
                // obtain next candidate for nooping
                currentPipeControlForNooping = nextCommandBuffer->pipeControlThatMayBeErasedLocation;
                // track epilogue pipe control
                epiloguePipeControlLocation = nextCommandBuffer->epiloguePipeControlLocation;

                flushStampUpdateHelper.insert(nextCommandBuffer->flushStamp->getStampReference());
                auto nextCommandBufferAddress = nextCommandBuffer->batchBuffer.commandBufferAllocation->getGpuAddress();
                auto offsetedCommandBuffer = (uint64_t)ptrOffset(nextCommandBufferAddress, nextCommandBuffer->batchBuffer.startOffset);
                auto cpuAddressForCommandBufferDestination = ptrOffset(nextCommandBuffer->batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), nextCommandBuffer->batchBuffer.startOffset);
                auto cpuAddressForCurrentCommandBufferEndingSection = alignUp(ptrOffset(currentBBendLocation, sizeof(MI_BATCH_BUFFER_START)), MemoryConstants::cacheLineSize);

                // if we point to exact same command buffer, then batch buffer start is not needed at all
                if (cpuAddressForCurrentCommandBufferEndingSection == cpuAddressForCommandBufferDestination) {
                    memset(currentBBendLocation, 0u, ptrDiff(cpuAddressForCurrentCommandBufferEndingSection, currentBBendLocation));
                } else {
                    addBatchBufferStart((MI_BATCH_BUFFER_START *)currentBBendLocation, offsetedCommandBuffer, false);
                }

                if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                    flatBatchBufferHelper->registerCommandChunk(nextCommandBuffer->batchBuffer, sizeof(MI_BATCH_BUFFER_START));
                }

                currentBBendLocation = nextCommandBuffer->batchBufferEndLocation;
                lastTaskCount = nextCommandBuffer->taskCount;
                lastPipeControlArgs = nextCommandBuffer->epiloguePipeControlArgs;
                nextCommandBuffer = nextCommandBuffer->next;

                commandBufferList.removeFrontOne();
            }
            surfacesForSubmit.reserve(resourcePackage.size() + 1);
            for (auto &surface : resourcePackage) {
                surfacesForSubmit.push_back(surface);
            }

            // make sure we flush DC if needed
            if (getDcFlushRequired(epiloguePipeControlLocation)) {
                lastPipeControlArgs.dcFlushEnable = true;

                if (debugManager.flags.DisableDcFlushInEpilogue.get()) {
                    lastPipeControlArgs.dcFlushEnable = false;
                }

                MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                    epiloguePipeControlLocation,
                    PostSyncMode::immediateData,
                    getTagAllocation()->getGpuAddress(),
                    lastTaskCount,
                    peekRootDeviceEnvironment(),
                    lastPipeControlArgs);
            }

            primaryCmdBuffer->batchBuffer.endCmdPtr = currentBBendLocation;

            if (this->flush(primaryCmdBuffer->batchBuffer, surfacesForSubmit) != SubmissionStatus::success) {
                submitResult = false;
                break;
            }

            // after flush task level is closed
            this->taskLevel++;

            flushStampUpdateHelper.updateAll(flushStamp->peekStamp());

            if (!isUpdateTagFromWaitEnabled()) {
                this->latestFlushedTaskCount = lastTaskCount;
            }

            this->makeSurfacePackNonResident(surfacesForSubmit, true);
            resourcePackage.clear();
        }
        this->totalMemoryUsed = 0;
    }

    return submitResult;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSize(const DispatchBcsFlags &dispatchBcsFlags) {
    size_t size = getCmdsSizeForHardwareContext() + sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);

    if (debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
        auto rootExecutionEnvironment = this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex].get();
        EncodeDummyBlitWaArgs waArgs{false, rootExecutionEnvironment};

        size += EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    }

    return size;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSizeAligned(const DispatchBcsFlags &dispatchBcsFlags) {
    return alignUp(getRequiredCmdStreamSize(dispatchBcsFlags), MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags, Device &device) {
    size_t size = getRequiredCmdStreamSize(dispatchFlags, device);
    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags, Device &device) {
    size_t size = getRequiredCmdSizeForPreamble(device);
    size += getRequiredStateBaseAddressSize(device);

    if (device.getDebugger()) {
        size += device.getDebugger()->getSbaTrackingCommandsSize(NEO::Debugger::SbaAddresses::trackedAddressCount);
    }
    if (!getSipSentFlag()) {
        size += PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(device, isRcs());
    }
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    size += sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);

    size += getCmdSizeForL3Config();
    if (this->streamProperties.stateComputeMode.isDirty()) {
        size += getCmdSizeForComputeMode();
    }
    size += getCmdSizeForMediaSampler(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    size += getCmdSizeForPipelineSelect();
    size += getCmdSizeForPreemption(dispatchFlags);
    if ((dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) || isRayTracingStateProgramingNeeded(device)) {
        size += getCmdSizeForPerDssBackedBuffer(device.getHardwareInfo());
    }
    size += getCmdSizeForEpilogue(dispatchFlags);
    size += getCmdsSizeForHardwareContext();
    if (csrSizeRequestFlags.activePartitionsChanged) {
        size += getCmdSizeForActivePartitionConfig();
    }

    if (executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
        if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }

    size += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.csrDependencies, false);
    size += TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<GfxFamily>(dispatchFlags.csrDependencies);

    if (dispatchFlags.isStallingCommandsOnNextFlushRequired) {
        size += getCmdSizeForStallingCommands(dispatchFlags);
    }

    if (requiresInstructionCacheFlush) {
        size += MemorySynchronizationCommands<GfxFamily>::getSizeForInstructionCacheFlush();
    }

    if (debugManager.flags.ForcePipeControlPriorToWalker.get()) {
        size += MemorySynchronizationCommands<GfxFamily>::getSizeForStallingBarrier();
        size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }

    return size;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPipelineSelect() const {
    size_t size = 0;
    if ((csrSizeRequestFlags.mediaSamplerConfigChanged ||
         csrSizeRequestFlags.systolicPipelineSelectMode ||
         !isPreambleSent) &&
        !isPipelineSelectAlreadyProgrammed()) {
        size += PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(peekRootDeviceEnvironment());
    }
    return size;
}

template <typename GfxFamily>
inline WaitStatus CommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(TaskCountType taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) {
    const auto params = kmdNotifyHelper->obtainTimeoutParams(useQuickKmdSleep, *getTagAddress(), taskCountToWait, flushStampToWait, throttle, this->isKmdWaitModeActive(),
                                                             this->isAnyDirectSubmissionEnabled());

    auto status = waitForCompletionWithTimeout(params, taskCountToWait);
    if (status == WaitStatus::notReady) {
        waitForFlushStamp(flushStampToWait);
        // now call blocking wait, this is to ensure that task count is reached
        status = waitForCompletionWithTimeout(WaitParams{false, false, false, 0}, taskCountToWait);
    }

    // If GPU hang occured, then propagate it to the caller.
    if (status == WaitStatus::gpuHang) {
        return status;
    }

    for (uint32_t i = 0; i < this->activePartitions; i++) {
        UNRECOVERABLE_IF(*(ptrOffset(getTagAddress(), (i * this->immWritePostSyncWriteOffset))) < taskCountToWait);
    }

    if (kmdNotifyHelper->quickKmdSleepForSporadicWaitsEnabled()) {
        kmdNotifyHelper->updateLastWaitForCompletionTimestamp();
    }
    return WaitStatus::ready;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags) {
    PreemptionHelper::programCmdStream<GfxFamily>(csr, dispatchFlags.preemptionMode, this->lastPreemptionMode, getPreemptionAllocation());
    this->lastPreemptionMode = dispatchFlags.preemptionMode;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPreemption(const DispatchFlags &dispatchFlags) const {
    return PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.preemptionMode, this->lastPreemptionMode);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStateSip(LinearStream &cmdStream, Device &device) {
    if (!this->isStateSipSent) {
        PreemptionHelper::programStateSip<GfxFamily>(cmdStream, device, this->osContext);
        setSipSentFlag(true);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programPreamble(LinearStream &csr, Device &device, uint32_t &newL3Config) {
    if (!this->isPreambleSent) {
        PreambleHelper<GfxFamily>::programPreamble(&csr, device, newL3Config, getPreemptionAllocation(), EngineHelpers::isBcs(osContext->getEngineType()));
        this->isPreambleSent = true;
        this->lastSentL3Config = newL3Config;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads) {
    if (mediaVfeStateDirty) {
        if (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::notApplicable) {
            lastAdditionalKernelExecInfo = dispatchFlags.additionalKernelExecInfo;
        }
        if (dispatchFlags.kernelExecutionType != KernelExecutionType::notApplicable) {
            lastKernelExecutionType = dispatchFlags.kernelExecutionType;
        }
        auto &hwInfo = peekHwInfo();

        auto isCooperative = dispatchFlags.kernelExecutionType == KernelExecutionType::concurrent;
        auto disableOverdispatch = (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::notSet);
        this->streamProperties.frontEndState.setPropertiesAll(isCooperative, dispatchFlags.disableEUFusion, disableOverdispatch);

        auto &gfxCoreHelper = getGfxCoreHelper();
        auto engineGroupType = gfxCoreHelper.getEngineGroupType(getOsContext().getEngineType(), getOsContext().getEngineUsage(), hwInfo);
        auto pVfeState = PreambleHelper<GfxFamily>::getSpaceForVfeState(&csr, hwInfo, engineGroupType);
        PreambleHelper<GfxFamily>::programVfeState(
            pVfeState, peekRootDeviceEnvironment(), requiredScratchSlot0Size, getScratchPatchAddress(),
            maxFrontEndThreads, streamProperties);
        auto commandOffset = PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(&csr, pVfeState);

        if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            flatBatchBufferHelper->collectScratchSpacePatchInfo(getScratchPatchAddress(), commandOffset, csr);
        }
        setMediaVFEStateDirty(false);
        this->streamProperties.frontEndState.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programMediaSampler(LinearStream &commandStream, DispatchFlags &dispatchFlags) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForMediaSampler(bool mediaSamplerRequired) const {
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::collectStateBaseAddresPatchInfo(
    uint64_t baseAddress,
    uint64_t commandOffset,
    const LinearStream *dsh,
    const LinearStream *ioh,
    const LinearStream *ssh,
    uint64_t generalStateBase,
    bool imagesSupported) {

    typedef typename GfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    if (imagesSupported) {
        UNRECOVERABLE_IF(!dsh);
        PatchInfoData dynamicStatePatchInfo = {dsh->getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::dynamicStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::defaultType};
        flatBatchBufferHelper->setPatchInfoData(dynamicStatePatchInfo);
    }
    PatchInfoData generalStatePatchInfo = {generalStateBase, 0u, PatchInfoAllocationType::generalStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::defaultType};
    PatchInfoData surfaceStatePatchInfo = {ssh->getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::surfaceStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::defaultType};

    flatBatchBufferHelper->setPatchInfoData(generalStatePatchInfo);
    flatBatchBufferHelper->setPatchInfoData(surfaceStatePatchInfo);
    collectStateBaseAddresIohPatchInfo(baseAddress, commandOffset, *ioh);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::resetKmdNotifyHelper(KmdNotifyHelper *newHelper) {
    kmdNotifyHelper.reset(newHelper);
    kmdNotifyHelper->updateAcLineStatus();
    if (kmdNotifyHelper->quickKmdSleepForSporadicWaitsEnabled()) {
        kmdNotifyHelper->updateLastWaitForCompletionTimestamp();
    }
}

template <typename GfxFamily>
uint64_t CommandStreamReceiverHw<GfxFamily>::getScratchPatchAddress() {
    return scratchSpaceController->getScratchPatchAddress();
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const {
    return debugManager.flags.ForceCsrReprogramming.get();
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::unregisterDirectSubmissionFromController() {
    auto directSubmissionController = executionEnvironment.directSubmissionController.get();
    if (directSubmissionController) {
        directSubmissionController->unregisterDirectSubmission(this);
    }
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::bcsRelaxedOrderingAllowed(const BlitPropertiesContainer &blitPropertiesContainer, bool hasStallingCmds) const {
    return directSubmissionRelaxedOrderingEnabled() && (debugManager.flags.DirectSubmissionRelaxedOrderingForBcs.get() == 1) &&
           (blitPropertiesContainer.size() == 1) && !hasStallingCmds;
}

template <typename GfxFamily>
uint32_t CommandStreamReceiverHw<GfxFamily>::getDirectSubmissionRelaxedOrderingQueueDepth() const {
    if (directSubmission.get()) {
        return directSubmission->getRelaxedOrderingQueueSize();
    }
    if (blitterDirectSubmission.get()) {
        return blitterDirectSubmission->getRelaxedOrderingQueueSize();
    }

    return 0;
}

template <typename GfxFamily>
TaskCountType CommandStreamReceiverHw<GfxFamily>::flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, Device &device) {
    auto lock = obtainUniqueOwnership();
    bool blitterDirectSubmission = this->isBlitterDirectSubmissionEnabled();
    auto debugPauseEnabled = PauseOnGpuProperties::featureEnabled(debugManager.flags.PauseOnBlitCopy.get());
    auto &rootDeviceEnvironment = this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex];

    const bool updateTag = !isUpdateTagFromWaitEnabled() || blocking;
    const bool hasStallingCmds = updateTag || !this->isEnginePrologueSent;
    const bool relaxedOrderingAllowed = bcsRelaxedOrderingAllowed(blitPropertiesContainer, hasStallingCmds);

    auto estimatedCsSize = BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitPropertiesContainer, blitPropertiesContainer[0].blitSyncProperties.isTimestampMode(), debugPauseEnabled, blitterDirectSubmission,
                                                                                   relaxedOrderingAllowed, *rootDeviceEnvironment.get());
    auto &commandStream = getCS(estimatedCsSize);

    auto commandStreamStart = commandStream.getUsed();
    auto newTaskCount = taskCount + 1;
    latestSentTaskCount = newTaskCount;

    this->initializeResources(false, device.getPreemptionMode());
    this->initDirectSubmission();

    if (PauseOnGpuProperties::pauseModeAllowed(debugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(),
                                                                  DebugPauseState::waitingForUserStartConfirmation,
                                                                  DebugPauseState::hasUserStartConfirmation, *rootDeviceEnvironment.get());
    }

    bool isRelaxedOrderingDispatch = false;

    if (relaxedOrderingAllowed) {
        uint32_t dependenciesCount = 0;

        for (auto timestampPacketContainer : blitPropertiesContainer[0].csrDependencies.timestampPacketContainer) {
            dependenciesCount += static_cast<uint32_t>(timestampPacketContainer->peekNodes().size());
        }

        isRelaxedOrderingDispatch = RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*this, dependenciesCount);
    }

    programEnginePrologue(commandStream);

    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = pageTableManager->initPageTableManagerRegisters(this);
    }

    if (isRelaxedOrderingDispatch) {
        RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(commandStream, true);
    }

    NEO::EncodeDummyBlitWaArgs waArgs{false, rootDeviceEnvironment.get()};
    MiFlushArgs args{waArgs};

    for (auto &blitProperties : blitPropertiesContainer) {
        TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, blitProperties.csrDependencies, isRelaxedOrderingDispatch, EngineHelpers::isBcs(this->osContext->getEngineType()));
        TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);

        BlitCommandsHelper<GfxFamily>::encodeWa(commandStream, blitProperties, latestSentBcsWaValue);

        if (blitProperties.blitSyncProperties.outputTimestampPacket && blitProperties.blitSyncProperties.isTimestampMode()) {
            BlitCommandsHelper<GfxFamily>::encodeProfilingStartMmios(commandStream, *blitProperties.blitSyncProperties.outputTimestampPacket);
        }

        if (debugManager.flags.FlushTlbBeforeCopy.get() == 1) {
            MiFlushArgs tlbFlushArgs{waArgs};
            tlbFlushArgs.commandWithPostSync = true;
            tlbFlushArgs.tlbFlush = true;

            EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, this->getGlobalFenceAllocation()->getGpuAddress(), 0, tlbFlushArgs);
        }

        BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(blitProperties, commandStream, *waArgs.rootDeviceEnvironment);

        if (blitProperties.blitSyncProperties.outputTimestampPacket) {
            bool deviceToHostPostSyncFenceRequired = getProductHelper().isDeviceToHostCopySignalingFenceRequired() &&
                                                     !blitProperties.dstAllocation->isAllocatedInLocalMemoryPool() &&
                                                     blitProperties.srcAllocation->isAllocatedInLocalMemoryPool();

            if (deviceToHostPostSyncFenceRequired) {
                MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), NEO::FenceType::release, peekRootDeviceEnvironment());
            }

            if (blitProperties.blitSyncProperties.isTimestampMode()) {
                EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, 0llu, newTaskCount, args);
                BlitCommandsHelper<GfxFamily>::encodeProfilingEndMmios(commandStream, *blitProperties.blitSyncProperties.outputTimestampPacket);
            } else {
                auto timestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*blitProperties.blitSyncProperties.outputTimestampPacket);
                args.commandWithPostSync = true;

                EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, timestampPacketGpuAddress, 0, args);
            }
            makeResident(*blitProperties.blitSyncProperties.outputTimestampPacket->getBaseGraphicsAllocation());
        }

        blitProperties.csrDependencies.makeResident(*this);
        blitProperties.srcAllocation->prepareHostPtrForResidency(this);
        blitProperties.dstAllocation->prepareHostPtrForResidency(this);
        makeResident(*blitProperties.srcAllocation);
        makeResident(*blitProperties.dstAllocation);
        if (blitProperties.clearColorAllocation) {
            makeResident(*blitProperties.clearColorAllocation);
        }
        if (blitProperties.multiRootDeviceEventSync != nullptr) {
            args.commandWithPostSync = true;
            args.notifyEnable = isUsedNotifyEnableForPostSync();
            EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, blitProperties.multiRootDeviceEventSync->getGpuAddress() + blitProperties.multiRootDeviceEventSync->getContextEndOffset(), std::numeric_limits<uint64_t>::max(), args);
        }
    }

    BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(commandStream);

    if (updateTag) {
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), NEO::FenceType::release, peekRootDeviceEnvironment());
        args.commandWithPostSync = true;
        args.waArgs.isWaRequired = true;
        args.notifyEnable = isUsedNotifyEnableForPostSync();

        args.tlbFlush |= (debugManager.flags.ForceTlbFlushWithTaskCountAfterCopy.get() == 1);

        EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, tagAllocation->getGpuAddress(), newTaskCount, args);
        auto dummyAllocation = rootDeviceEnvironment->getDummyAllocation();
        if (dummyAllocation) {
            makeResident(*dummyAllocation);
        }

        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), NEO::FenceType::release, peekRootDeviceEnvironment());
    }
    if (PauseOnGpuProperties::pauseModeAllowed(debugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(),
                                                                  DebugPauseState::waitingForUserEndConfirmation,
                                                                  DebugPauseState::hasUserEndConfirmation, *rootDeviceEnvironment.get());
    }

    void *endingCmdPtr = nullptr;
    programEndingCmd(commandStream, &endingCmdPtr, blitterDirectSubmission, isRelaxedOrderingDispatch, true);

    EncodeNoop<GfxFamily>::alignToCacheLine(commandStream);

    makeResident(*tagAllocation);
    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    uint64_t taskStartAddress = commandStream.getGpuBase() + commandStreamStart;

    BatchBuffer batchBuffer{commandStream.getGraphicsAllocation(), commandStreamStart, 0, taskStartAddress, nullptr, false, getThrottleFromPowerSavingUint(this->getUmdPowerHintValue()), QueueSliceCount::defaultSliceCount,
                            commandStream.getUsed(), &commandStream, endingCmdPtr, this->getNumClients(), hasStallingCmds, isRelaxedOrderingDispatch, blocking, false};

    updateStreamTaskCount(commandStream, newTaskCount);

    auto flushSubmissionStatus = flush(batchBuffer, getResidencyAllocations());
    if (flushSubmissionStatus != SubmissionStatus::success) {
        updateStreamTaskCount(commandStream, taskCount);
        return CompletionStamp::getTaskCountFromSubmissionStatusError(flushSubmissionStatus);
    }
    makeSurfacePackNonResident(getResidencyAllocations(), true);

    if (updateTag) {
        latestFlushedTaskCount = newTaskCount;
    }

    taskCount = newTaskCount;
    auto flushStampToWait = flushStamp->peekStamp();

    lock.unlock();
    if (blocking) {
        const auto waitStatus = waitForTaskCountWithKmdNotifyFallback(newTaskCount, flushStampToWait, false, getThrottleFromPowerSavingUint(this->getUmdPowerHintValue()));
        internalAllocationStorage->cleanAllocationList(newTaskCount, TEMPORARY_ALLOCATION);
        internalAllocationStorage->cleanAllocationList(newTaskCount, DEFERRED_DEALLOCATION);

        if (waitStatus == WaitStatus::gpuHang) {
            return CompletionStamp::gpuHang;
        }
    }

    return newTaskCount;
}

template <typename GfxFamily>
inline SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushTagUpdate() {
    if (this->osContext != nullptr) {
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            return this->flushMiFlushDW(false);
        } else {
            return this->flushPipeControl(false);
        }
    }
    return SubmissionStatus::deviceUninitialized;
}

template <typename GfxFamily>
inline SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushMiFlushDW(bool initializeProlog) {
    auto lock = obtainUniqueOwnership();

    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&peekRootDeviceEnvironment())};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = isUsedNotifyEnableForPostSync();

    size_t requiredSize = MemorySynchronizationCommands<GfxFamily>::getSizeForSingleAdditionalSynchronization(NEO::FenceType::release, peekRootDeviceEnvironment()) +
                          EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);

    if (initializeProlog) {
        requiredSize += getCmdsSizeForHardwareContext();
    }

    auto &commandStream = getCS(requiredSize);
    auto commandStreamStart = commandStream.getUsed();

    if (initializeProlog) {
        programHardwareContext(commandStream);
    }

    NEO::MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, 0, NEO::FenceType::release, peekRootDeviceEnvironment());

    EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, tagAllocation->getGpuAddress(), taskCount + 1, args);

    makeResident(*tagAllocation);

    auto submissionStatus = this->flushSmallTask(commandStream, commandStreamStart);
    this->latestFlushedTaskCount = taskCount.load();
    return submissionStatus;
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushPipeControl(bool stateCacheFlush) {
    auto lock = obtainUniqueOwnership();

    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport;
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    args.workloadPartitionOffset = isMultiTileOperationEnabled();

    if (stateCacheFlush) {
        args.textureCacheInvalidationEnable = true;
        args.renderTargetCacheFlushEnable = true;
        args.stateCacheInvalidationEnable = true;
        args.tlbInvalidation = this->isTlbFlushRequiredForStateCacheFlush();
    }

    auto dispatchSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) + this->getCmdSizeForPrologue();

    auto &commandStream = getCS(dispatchSize);
    auto commandStreamStart = commandStream.getUsed();

    this->programEnginePrologue(commandStream);

    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(commandStream,
                                                                              PostSyncMode::immediateData,
                                                                              getTagAllocation()->getGpuAddress(),
                                                                              taskCount + 1,
                                                                              peekRootDeviceEnvironment(),
                                                                              args);

    makeResident(*tagAllocation);
    makeResident(*commandStream.getGraphicsAllocation());

    auto submissionStatus = this->flushSmallTask(commandStream, commandStreamStart);
    this->latestFlushedTaskCount = taskCount.load();
    return submissionStatus;
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushSmallTask(LinearStream &commandStreamTask, size_t commandStreamStartTask) {
    void *endingCmdPtr = nullptr;
    programEndingCmd(commandStreamTask, &endingCmdPtr, isAnyDirectSubmissionEnabled(), false, EngineHelpers::isBcs(this->osContext->getEngineType()));

    auto bytesToPad = EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize() -
                      EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferEndSize();
    EncodeNoop<GfxFamily>::emitNoop(commandStreamTask, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamTask);

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    uint64_t taskStartAddress = commandStreamTask.getGpuBase() + commandStreamStartTask;

    BatchBuffer batchBuffer{commandStreamTask.getGraphicsAllocation(), commandStreamStartTask, 0, taskStartAddress,
                            nullptr, false, getThrottleFromPowerSavingUint(this->getUmdPowerHintValue()), QueueSliceCount::defaultSliceCount,
                            commandStreamTask.getUsed(), &commandStreamTask, endingCmdPtr, this->getNumClients(), true, false, true, true};

    this->latestSentTaskCount = taskCount + 1;
    auto submissionStatus = flushHandler(batchBuffer, getResidencyAllocations());
    if (submissionStatus == SubmissionStatus::success) {
        taskCount++;
    }
    return submissionStatus;
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::sendRenderStateCacheFlush() {
    return this->flushPipeControl(true);
}

template <typename GfxFamily>
inline SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    this->latestFlushIsTaskCountUpdateOnly = batchBuffer.taskCountUpdateOnly;
    auto status = flush(batchBuffer, allocationsForResidency);
    makeSurfacePackNonResident(allocationsForResidency, true);
    return status;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isUpdateTagFromWaitEnabled() {
    auto &gfxCoreHelper = getGfxCoreHelper();
    auto &productHelper = this->peekRootDeviceEnvironment().template getHelper<ProductHelper>();

    auto enabled = gfxCoreHelper.isUpdateTaskCountFromWaitSupported();

    if (productHelper.isL3FlushAfterPostSyncRequired(this->heaplessModeEnabled) && ApiSpecificConfig::isUpdateTagFromWaitEnabledForHeapless()) {
        enabled &= true;
    } else {
        enabled &= this->isAnyDirectSubmissionEnabled();
    }

    switch (debugManager.flags.UpdateTaskCountFromWait.get()) {
    case 0:
        enabled = false;
        break;
    case 1:
        enabled = this->isDirectSubmissionEnabled();
        break;
    case 2:
        enabled = this->isAnyDirectSubmissionEnabled();
        break;
    case 3:
        enabled = true;
        break;
    }

    return enabled;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::updateTagFromWait() {
    flushBatchedSubmissions();
    if (isUpdateTagFromWaitEnabled()) {
        flushTagUpdate();
    }
}

template <typename GfxFamily>
inline MemoryCompressionState CommandStreamReceiverHw<GfxFamily>::getMemoryCompressionState(bool auxTranslationRequired) const {
    return MemoryCompressionState::notApplicable;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isPipelineSelectAlreadyProgrammed() const {
    const auto &productHelper = getProductHelper();
    return this->streamProperties.stateComputeMode.isDirty() && productHelper.is3DPipelineSelectWARequired() && isRcs();
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEpilogue(LinearStream &csr, Device &device, void **batchBufferEndLocation, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.epilogueRequired) {
        auto currentOffset = ptrDiff(csr.getSpace(0u), csr.getCpuBase());
        auto gpuAddress = ptrOffset(csr.getGraphicsAllocation()->getGpuAddress(), currentOffset);

        addBatchBufferStart(reinterpret_cast<typename GfxFamily::MI_BATCH_BUFFER_START *>(*batchBufferEndLocation), gpuAddress, false);
        this->programEpliogueCommands(csr, dispatchFlags);
        programEndingCmd(csr, batchBufferEndLocation, isDirectSubmissionEnabled(), false, EngineHelpers::isBcs(this->osContext->getEngineType()));
        EncodeNoop<GfxFamily>::alignToCacheLine(csr);
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForEpilogue(const DispatchFlags &dispatchFlags) const {
    if (dispatchFlags.epilogueRequired) {
        size_t terminateCmd = sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);
        if (isDirectSubmissionEnabled()) {
            terminateCmd = sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);
        }
        auto size = getCmdSizeForEpilogueCommands(dispatchFlags) + terminateCmd;
        return alignUp(size, MemoryConstants::cacheLineSize);
    }
    return 0u;
}
template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEnginePrologue(LinearStream &csr) {
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPrologue() const {
    return 0u;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programExceptions(LinearStream &csr, Device &device) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForExceptions() const {
    return 0u;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::stopDirectSubmission(bool blocking, bool needsLock) {
    if (this->isAnyDirectSubmissionEnabled()) {
        std::unique_lock<MutexType> lock;
        if (needsLock) {
            lock = obtainUniqueOwnership();
        }
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            this->blitterDirectSubmission->stopRingBuffer(blocking);
        } else {
            this->directSubmission->stopRingBuffer(blocking);
        }
    }
}

template <typename GfxFamily>
inline QueueThrottle CommandStreamReceiverHw<GfxFamily>::getLastDirectSubmissionThrottle() {
    if (this->isAnyDirectSubmissionEnabled()) {
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            return this->blitterDirectSubmission->getLastSubmittedThrottle();
        } else {
            return this->directSubmission->getLastSubmittedThrottle();
        }
    }
    return QueueThrottle::MEDIUM;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::initDirectSubmission() {
    bool ret = true;

    bool submitOnInit = false;
    auto startDirect = this->osContext->isDirectSubmissionAvailable(peekHwInfo(), submitOnInit);

    if (startDirect) {
        if (!this->isAnyDirectSubmissionEnabled()) {
            auto lock = this->obtainUniqueOwnership();
            if (!this->isAnyDirectSubmissionEnabled()) {
                if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
                    blitterDirectSubmission = DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::create(*this);
                    ret = blitterDirectSubmission->initialize(submitOnInit);
                    completionFenceValuePointer = blitterDirectSubmission->getCompletionValuePointer();

                } else {
                    directSubmission = DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::create(*this);
                    ret = directSubmission->initialize(submitOnInit);
                    completionFenceValuePointer = directSubmission->getCompletionValuePointer();
                }
                auto directSubmissionController = executionEnvironment.initializeDirectSubmissionController();
                if (directSubmissionController) {
                    directSubmissionController->registerDirectSubmission(this);
                }
                this->startControllingDirectSubmissions();
                if (this->isUpdateTagFromWaitEnabled()) {
                    this->overrideDispatchPolicy(DispatchMode::immediateDispatch);
                }
            }
            this->osContext->setDirectSubmissionActive();
            if (this->osContext->isDirectSubmissionLightActive()) {
                this->pushAllocationsForMakeResident = false;
                WaitUtils::init(WaitUtils::WaitpkgUse::tpause, *this->peekExecutionEnvironment().rootDeviceEnvironments[this->getRootDeviceIndex()]->getHardwareInfo());
                WaitUtils::adjustWaitpkgParamsForUllsLight();
            }
        }
    }
    return ret;
}

template <typename GfxFamily>
TagAllocatorBase *CommandStreamReceiverHw<GfxFamily>::getTimestampPacketAllocator() {
    if (timestampPacketAllocator.get() == nullptr) {
        auto &gfxCoreHelper = getGfxCoreHelper();
        const RootDeviceIndicesContainer rootDeviceIndices = {rootDeviceIndex};

        timestampPacketAllocator = gfxCoreHelper.createTimestampPacketAllocator(rootDeviceIndices, getMemoryManager(), getPreferredTagPoolSize(), getType(), osContext->getDeviceBitfield());
    }
    return timestampPacketAllocator.get();
}

template <typename GfxFamily>
std::unique_ptr<TagAllocatorBase> CommandStreamReceiverHw<GfxFamily>::createMultiRootDeviceTimestampPacketAllocator(const RootDeviceIndicesContainer &rootDeviceIndices) {
    auto &gfxCoreHelper = getGfxCoreHelper();
    return gfxCoreHelper.createTimestampPacketAllocator(rootDeviceIndices, getMemoryManager(), getPreferredTagPoolSize(), getType(), osContext->getDeviceBitfield());
}
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::postInitFlagsSetup() {
    useNewResourceImplicitFlush = checkPlatformSupportsNewResourceImplicitFlush();
    int32_t overrideNewResourceImplicitFlush = debugManager.flags.PerformImplicitFlushForNewResource.get();
    if (overrideNewResourceImplicitFlush != -1) {
        useNewResourceImplicitFlush = overrideNewResourceImplicitFlush == 0 ? false : true;
    }
    useGpuIdleImplicitFlush = checkPlatformSupportsGpuIdleImplicitFlush();
    int32_t overrideGpuIdleImplicitFlush = debugManager.flags.PerformImplicitFlushForIdleGpu.get();
    if (overrideGpuIdleImplicitFlush != -1) {
        useGpuIdleImplicitFlush = overrideGpuIdleImplicitFlush == 0 ? false : true;
    }
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingCommands(const DispatchFlags &dispatchFlags) const {
    auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;
    if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() > 0) {
        return getCmdSizeForStallingPostSyncCommands();
    } else {
        return getCmdSizeForStallingNoPostSyncCommands();
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programActivePartitionConfigFlushTask(LinearStream &csr) {
    if (csrSizeRequestFlags.activePartitionsChanged) {
        programActivePartitionConfig(csr);
    }
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::hasSharedHandles() {
    if (!csrSizeRequestFlags.hasSharedHandles) {
        for (const auto &allocation : this->getResidencyAllocations()) {
            if (allocation->peekSharedHandle()) {
                csrSizeRequestFlags.hasSharedHandles = true;
                break;
            }
        }
    }
    return csrSizeRequestFlags.hasSharedHandles;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForComputeMode() {
    return EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(this->peekRootDeviceEnvironment(), hasSharedHandles(), isRcs());
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleFrontEndStateTransition(const DispatchFlags &dispatchFlags) {
    if (streamProperties.frontEndState.disableOverdispatch.value != -1) {
        lastAdditionalKernelExecInfo = streamProperties.frontEndState.disableOverdispatch.value == 1 ? AdditionalKernelExecInfo::disableOverdispatch : AdditionalKernelExecInfo::notSet;
    }
    if (streamProperties.frontEndState.computeDispatchAllWalkerEnable.value != -1) {
        lastKernelExecutionType = streamProperties.frontEndState.computeDispatchAllWalkerEnable.value == 1 ? KernelExecutionType::concurrent : KernelExecutionType::defaultType;
    }

    if (feSupportFlags.disableOverdispatch &&
        (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::notApplicable && lastAdditionalKernelExecInfo != dispatchFlags.additionalKernelExecInfo)) {
        setMediaVFEStateDirty(true);
    }

    if (feSupportFlags.computeDispatchAllWalker &&
        (dispatchFlags.kernelExecutionType != KernelExecutionType::notApplicable && lastKernelExecutionType != dispatchFlags.kernelExecutionType)) {
        setMediaVFEStateDirty(true);
    }

    if (feSupportFlags.disableEuFusion &&
        (streamProperties.frontEndState.disableEUFusion.value == -1 || dispatchFlags.disableEUFusion != !!streamProperties.frontEndState.disableEUFusion.value)) {
        setMediaVFEStateDirty(true);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handlePipelineSelectStateTransition(const DispatchFlags &dispatchFlags) {
    if (streamProperties.pipelineSelect.mediaSamplerDopClockGate.value != -1) {
        this->lastMediaSamplerConfig = static_cast<int8_t>(streamProperties.pipelineSelect.mediaSamplerDopClockGate.value);
    }
    if (streamProperties.pipelineSelect.systolicMode.value != -1) {
        this->lastSystolicPipelineSelectMode = !!streamProperties.pipelineSelect.systolicMode.value;
    }

    csrSizeRequestFlags.mediaSamplerConfigChanged = this->pipelineSupportFlags.mediaSamplerDopClockGate &&
                                                    (this->lastMediaSamplerConfig != static_cast<int8_t>(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired));
    csrSizeRequestFlags.systolicPipelineSelectMode = this->pipelineSupportFlags.systolicMode &&
                                                     (this->lastSystolicPipelineSelectMode != dispatchFlags.pipelineSelectArgs.systolicPipelineSelectMode);
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::directSubmissionRelaxedOrderingEnabled() const {
    return ((directSubmission.get() && directSubmission->isRelaxedOrderingEnabled()) ||
            (blitterDirectSubmission.get() && blitterDirectSubmission->isRelaxedOrderingEnabled()));
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::handleStateBaseAddressStateTransition(const DispatchFlags &dispatchFlags, bool &isStateBaseAddressDirty) {
    auto &rootDeviceEnvironment = this->peekRootDeviceEnvironment();

    if (this->streamProperties.stateBaseAddress.statelessMocs.value != -1) {
        this->latestSentStatelessMocsConfig = static_cast<uint32_t>(this->streamProperties.stateBaseAddress.statelessMocs.value);
    }
    auto mocsIndex = this->latestSentStatelessMocsConfig;
    if (dispatchFlags.l3CacheSettings != L3CachingSettings::notApplicable) {
        auto l3On = dispatchFlags.l3CacheSettings != L3CachingSettings::l3CacheOff;
        auto l1On = dispatchFlags.l3CacheSettings == L3CachingSettings::l3AndL1On;

        auto &gfxCoreHelper = getGfxCoreHelper();
        mocsIndex = gfxCoreHelper.getMocsIndex(*rootDeviceEnvironment.getGmmHelper(), l3On, l1On);
    }
    if (mocsIndex != this->latestSentStatelessMocsConfig) {
        isStateBaseAddressDirty = true;
        this->latestSentStatelessMocsConfig = mocsIndex;
    }
    this->streamProperties.stateBaseAddress.setPropertyStatelessMocs(mocsIndex);

    auto memoryCompressionState = this->lastMemoryCompressionState;
    if (dispatchFlags.memoryCompressionState != MemoryCompressionState::notApplicable) {
        memoryCompressionState = dispatchFlags.memoryCompressionState;
    }
    if (memoryCompressionState != this->lastMemoryCompressionState) {
        isStateBaseAddressDirty = true;
        this->lastMemoryCompressionState = memoryCompressionState;
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::updateStreamTaskCount(LinearStream &stream, TaskCountType newTaskCount) {
    stream.getGraphicsAllocation()->updateTaskCount(newTaskCount, this->osContext->getContextId());
    stream.getGraphicsAllocation()->updateResidencyTaskCount(newTaskCount, this->osContext->getContextId());
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStateBaseAddress(const IndirectHeap *dsh,
                                                                        const IndirectHeap *ioh,
                                                                        const IndirectHeap *ssh,
                                                                        DispatchFlags &dispatchFlags,
                                                                        Device &device,
                                                                        LinearStream &commandStreamCSR,
                                                                        bool stateBaseAddressDirty) {

    const auto bindlessHeapsHelper = device.getBindlessHeapsHelper();
    const bool useGlobalHeaps = bindlessHeapsHelper != nullptr;

    auto &hwInfo = this->peekHwInfo();

    const bool hasDsh = hwInfo.capabilityTable.supportsImages && dsh != nullptr;
    int64_t dynamicStateBaseAddress = 0;
    size_t dynamicStateSize = 0;
    if (hasDsh) {
        dynamicStateBaseAddress = NEO::getStateBaseAddress(*dsh, useGlobalHeaps);
        dynamicStateSize = NEO::getStateSize(*dsh, bindlessHeapsHelper);
    }

    int64_t surfaceStateBaseAddress = 0;
    size_t surfaceStateSize = 0;
    if (ssh != nullptr) {
        surfaceStateBaseAddress = NEO::getStateBaseAddressForSsh(*ssh, useGlobalHeaps);
        surfaceStateSize = NEO::getStateSizeForSsh(*ssh, useGlobalHeaps);
    }

    bool dshDirty = hasDsh ? dshState.updateAndCheck(dsh, dynamicStateBaseAddress, dynamicStateSize) : false;
    bool iohDirty = iohState.updateAndCheck(ioh);
    bool sshDirty = ssh != nullptr ? sshState.updateAndCheck(ssh, surfaceStateBaseAddress, surfaceStateSize) : false;

    bool bindingTablePoolCommandNeeded = sshDirty && (ssh->getGraphicsAllocation() != globalStatelessHeapAllocation);

    if (dshDirty) {
        this->streamProperties.stateBaseAddress.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    }
    if (iohDirty) {
        int64_t indirectObjectBaseAddress = ioh->getHeapGpuBase();
        size_t indirectObjectSize = ioh->getHeapSizeInPages();
        this->streamProperties.stateBaseAddress.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    }
    if (sshDirty) {
        int64_t bindingTablePoolBaseAddress = -1;
        size_t bindingTablePoolSize = std::numeric_limits<size_t>::max();

        if (bindingTablePoolCommandNeeded) {
            bindingTablePoolBaseAddress = surfaceStateBaseAddress;
            bindingTablePoolSize = surfaceStateSize;
        }
        this->streamProperties.stateBaseAddress.setPropertiesBindingTableSurfaceState(bindingTablePoolBaseAddress, bindingTablePoolSize,
                                                                                      surfaceStateBaseAddress, surfaceStateSize);
    }

    auto force32BitAllocations = getMemoryManager()->peekForce32BitAllocations();
    stateBaseAddressDirty |= ((gsbaFor32BitProgrammed ^ dispatchFlags.gsba32BitRequired) && force32BitAllocations);
    bool isStateBaseAddressDirty = dshDirty || iohDirty || sshDirty || stateBaseAddressDirty;
    handleStateBaseAddressStateTransition(dispatchFlags, isStateBaseAddressDirty);

    // reprogram state base address command if required
    if (isStateBaseAddressDirty) {
        reprogramStateBaseAddress(dsh, ioh, ssh, dispatchFlags, device, commandStreamCSR, force32BitAllocations, sshDirty, bindingTablePoolCommandNeeded);
    }

    if (hasDsh) {
        auto dshAllocation = dsh->getGraphicsAllocation();
        this->makeResident(*dshAllocation);
        dshAllocation->setEvictable(false);
    }

    if (ssh != nullptr) {
        auto sshAllocation = ssh->getGraphicsAllocation();
        this->makeResident(*sshAllocation);
    }

    auto iohAllocation = ioh->getGraphicsAllocation();
    this->makeResident(*iohAllocation);
    iohAllocation->setEvictable(false);

    if (globalStatelessHeapAllocation) {
        makeResident(*globalStatelessHeapAllocation);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::reprogramStateBaseAddress(const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh, DispatchFlags &dispatchFlags, Device &device, LinearStream &commandStreamCSR, bool force32BitAllocations, bool sshDirty, bool bindingTablePoolCommandNeeded) {

    uint64_t newGshBase = 0;
    gsbaFor32BitProgrammed = false;
    if (is64bit && scratchSpaceController->getScratchSpaceSlot0Allocation() && !force32BitAllocations) {
        newGshBase = scratchSpaceController->calculateNewGSH();
    } else if (is64bit && force32BitAllocations && dispatchFlags.gsba32BitRequired) {
        bool useLocalMemory = scratchSpaceController->getScratchSpaceSlot0Allocation() ? scratchSpaceController->getScratchSpaceSlot0Allocation()->isAllocatedInLocalMemoryPool() : false;
        newGshBase = getMemoryManager()->getExternalHeapBaseAddress(rootDeviceIndex, useLocalMemory);
        gsbaFor32BitProgrammed = true;
    }

    uint64_t indirectObjectStateBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, ioh->getGraphicsAllocation()->isAllocatedInLocalMemoryPool());

    if (sshDirty) {
        bindingTableBaseAddressRequired = bindingTablePoolCommandNeeded;
    }

    programStateBaseAddressCommon(dsh, ioh, ssh, nullptr,
                                  newGshBase,
                                  indirectObjectStateBaseAddress,
                                  dispatchFlags.pipelineSelectArgs,
                                  device,
                                  commandStreamCSR,
                                  bindingTableBaseAddressRequired,
                                  dispatchFlags.areMultipleSubDevicesInContext,
                                  true);
    bindingTableBaseAddressRequired = false;

    setGSBAStateDirty(false);
    this->streamProperties.stateBaseAddress.clearIsDirty();
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStateBaseAddressCommon(
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
    StateBaseAddressProperties *sbaProperties,
    uint64_t generalStateBaseAddress,
    uint64_t indirectObjectStateBaseAddress,
    PipelineSelectArgs &pipelineSelectArgs,
    Device &device,
    LinearStream &csrCommandStream,
    bool dispatchBindingTableCommand,
    bool areMultipleSubDevicesInContext,
    bool setGeneralStateBaseAddress) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    auto &rootDeviceEnvironment = this->peekRootDeviceEnvironment();
    bool debuggingEnabled = device.getDebugger() != nullptr && !this->osContext->isInternalEngine();

    EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(csrCommandStream, rootDeviceEnvironment, isRcs(), this->dcFlushSupport);
    EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(csrCommandStream, pipelineSelectArgs, true, rootDeviceEnvironment, isRcs());

    auto stateBaseAddressCmdOffset = csrCommandStream.getUsed();
    auto instructionHeapBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));
    auto bindlessSurfStateBase = 0ull;
    auto globalHeapsBase = 0ull;
    bool useGlobalSshAndDsh = false;

    if (device.getBindlessHeapsHelper()) {
        bindlessSurfStateBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        globalHeapsBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
        useGlobalSshAndDsh = true;
    }

    STATE_BASE_ADDRESS stateBaseAddressCmd;
    StateBaseAddressHelperArgs<GfxFamily> args = {
        generalStateBaseAddress,                  // generalStateBaseAddress
        indirectObjectStateBaseAddress,           // indirectObjectHeapBaseAddress
        instructionHeapBaseAddress,               // instructionHeapBaseAddress
        globalHeapsBase,                          // globalHeapsBaseAddress
        0,                                        // surfaceStateBaseAddress
        bindlessSurfStateBase,                    // bindlessSurfaceStateBaseAddress
        &stateBaseAddressCmd,                     // stateBaseAddressCmd
        sbaProperties,                            // sbaProperties
        dsh,                                      // dsh
        ioh,                                      // ioh
        ssh,                                      // ssh
        device.getGmmHelper(),                    // gmmHelper
        this->latestSentStatelessMocsConfig,      // statelessMocsIndex
        l1CachePolicyData.getL1CacheValue(false), // l1CachePolicy
        l1CachePolicyData.getL1CacheValue(true),  // l1CachePolicyDebuggerActive
        this->lastMemoryCompressionState,         // memoryCompressionState
        true,                                     // setInstructionStateBaseAddress
        setGeneralStateBaseAddress,               // setGeneralStateBaseAddress
        useGlobalSshAndDsh,                       // useGlobalHeapsBaseAddress
        isMultiOsContextCapable(),                // isMultiOsContextCapable
        areMultipleSubDevicesInContext,           // areMultipleSubDevicesInContext
        false,                                    // overrideSurfaceStateBaseAddress
        debuggingEnabled,                         // isDebuggerActive
        this->doubleSbaWa,                        // doubleSbaWa
        this->heaplessModeEnabled                 // heaplessModeEnabled
    };

    StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(args, csrCommandStream);

    bool sbaTrackingEnabled = debuggingEnabled;
    if (sbaTrackingEnabled) {
        device.getL0Debugger()->programSbaAddressLoad(csrCommandStream,
                                                      device.getL0Debugger()->getSbaTrackingBuffer(this->getOsContext().getContextId())->getGpuAddress(),
                                                      EngineHelpers::isBcs(this->osContext->getEngineType()));
    }

    NEO::EncodeStateBaseAddress<GfxFamily>::setSbaTrackingForL0DebuggerIfEnabled(sbaTrackingEnabled,
                                                                                 device,
                                                                                 csrCommandStream,
                                                                                 stateBaseAddressCmd, true);

    if (dispatchBindingTableCommand) {
        uint64_t bindingTableBaseAddress = 0;
        uint32_t bindingTableSize = 0;
        if (sbaProperties) {
            bindingTableBaseAddress = static_cast<uint64_t>(sbaProperties->bindingTablePoolBaseAddress.value);
            bindingTableSize = static_cast<uint32_t>(sbaProperties->bindingTablePoolSize.value);
        } else {
            UNRECOVERABLE_IF(!ssh);
            bindingTableBaseAddress = ssh->getHeapGpuBase();
            bindingTableSize = ssh->getHeapSizeInPages();
        }
        StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(csrCommandStream, bindingTableBaseAddress, bindingTableSize, device.getGmmHelper());
    }

    EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(csrCommandStream, pipelineSelectArgs, false, rootDeviceEnvironment, isRcs());

    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        collectStateBaseAddresPatchInfo(commandStream.getGraphicsAllocation()->getGpuAddress(), stateBaseAddressCmdOffset, dsh, ioh, ssh, generalStateBaseAddress,
                                        device.getDeviceInfo().imageSupport);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::processBarrierWithPostSync(LinearStream &commandStreamTask, DispatchFlags &dispatchFlags, bool &levelClosed, void *&currentPipeControlForNooping, void *&epiloguePipeControlLocation, bool &hasStallingCmdsOnTaskStream, PipeControlArgs &args) {

    if (this->dispatchMode == DispatchMode::immediateDispatch) {
        // for ImmediateDispatch we will send this right away, therefore this pipe control will close the level
        // for BatchedSubmissions it will be nooped and only last ppc in batch will be emitted.
        levelClosed = true;
        // if we guard with ppc, flush dc as well to speed up completion latency
        if (dispatchFlags.guardCommandBufferWithPipeControl || this->heapStorageRequiresRecyclingTag || dispatchFlags.blocking) {
            dispatchFlags.dcFlush = this->dcFlushSupport;
        }
    }

    epiloguePipeControlLocation = ptrOffset(commandStreamTask.getCpuBase(), commandStreamTask.getUsed());

    if ((dispatchFlags.outOfOrderExecutionAllowed || timestampPacketWriteEnabled) &&
        !dispatchFlags.dcFlush) {
        currentPipeControlForNooping = epiloguePipeControlLocation;
    }

    hasStallingCmdsOnTaskStream = true;

    auto address = getTagAllocation()->getGpuAddress();
    auto &rootDeviceEnvironment = this->peekRootDeviceEnvironment();

    args.dcFlushEnable = getDcFlushRequired(dispatchFlags.dcFlush);
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    args.tlbInvalidation |= dispatchFlags.memoryMigrationRequired;
    args.textureCacheInvalidationEnable |= dispatchFlags.textureCacheFlush || this->heapStorageRequiresRecyclingTag;
    args.workloadPartitionOffset = isMultiTileOperationEnabled();
    args.stateCacheInvalidationEnable |= dispatchFlags.stateCacheInvalidation || this->heapStorageRequiresRecyclingTag;
    this->heapStorageRequiresRecyclingTag = false;

    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        commandStreamTask,
        PostSyncMode::immediateData,
        address,
        taskCount + 1,
        rootDeviceEnvironment,
        args);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", peekTaskCount());
    if (debugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        flatBatchBufferHelper->setPatchInfoData(PatchInfoData(address, 0u,
                                                              PatchInfoAllocationType::tagAddress,
                                                              commandStreamTask.getGraphicsAllocation()->getGpuAddress(),
                                                              commandStreamTask.getUsed() - 2 * sizeof(uint64_t),
                                                              PatchInfoAllocationType::defaultType));
        flatBatchBufferHelper->setPatchInfoData(PatchInfoData(address, 0u,
                                                              PatchInfoAllocationType::tagValue,
                                                              commandStreamTask.getGraphicsAllocation()->getGpuAddress(),
                                                              commandStreamTask.getUsed() - sizeof(uint64_t),
                                                              PatchInfoAllocationType::defaultType));
    }
}

template <typename GfxFamily>
inline CompletionStamp CommandStreamReceiverHw<GfxFamily>::handleFlushTaskSubmission(BatchBuffer &&batchBuffer,
                                                                                     const DispatchFlags &dispatchFlags,
                                                                                     Device &device,
                                                                                     void *currentPipeControlForNooping,
                                                                                     void *epiloguePipeControlLocation,
                                                                                     PipeControlArgs &args,
                                                                                     bool submitTask,
                                                                                     bool submitCSR,
                                                                                     bool hasStallingCmdsOnTaskStream,
                                                                                     bool levelClosed,
                                                                                     bool implicitFlush) {

    if (submitCSR || submitTask) {
        if (this->dispatchMode == DispatchMode::immediateDispatch) {
            auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
            if (submissionStatus != SubmissionStatus::success) {
                updateStreamTaskCount(*batchBuffer.stream, taskCount);
                CompletionStamp completionStamp = {CompletionStamp::getTaskCountFromSubmissionStatusError(submissionStatus)};
                return completionStamp;
            }
            if (hasStallingCmdsOnTaskStream) {
                this->latestFlushedTaskCount = this->taskCount + 1;
            }
        } else {
            auto commandBuffer = new CommandBuffer(device);
            commandBuffer->batchBufferEndLocation = batchBuffer.endCmdPtr;
            commandBuffer->batchBuffer = std::move(batchBuffer);
            commandBuffer->surfaces.swap(this->getResidencyAllocations());
            commandBuffer->taskCount = this->taskCount + 1;
            commandBuffer->flushStamp->replaceStampObject(dispatchFlags.flushStampReference);
            commandBuffer->pipeControlThatMayBeErasedLocation = currentPipeControlForNooping;
            commandBuffer->epiloguePipeControlLocation = epiloguePipeControlLocation;
            commandBuffer->epiloguePipeControlArgs = args;
            this->submissionAggregator->recordCommandBuffer(commandBuffer);
        }
    } else {
        this->makeSurfacePackNonResident(this->getResidencyAllocations(), true);
    }

    if (this->dispatchMode == DispatchMode::batchedDispatch) {
        handleBatchedDispatchImplicitFlush(device.getDeviceInfo().globalMemSize, implicitFlush);
    }

    CompletionStamp completionStamp = updateTaskCountAndGetCompletionStamp(levelClosed);
    return completionStamp;
}

template <typename GfxFamily>
inline CompletionStamp CommandStreamReceiverHw<GfxFamily>::updateTaskCountAndGetCompletionStamp(bool levelClosed) {

    ++taskCount;

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", peekTaskCount());
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", tagAddress ? *tagAddress : 0);

    CompletionStamp completionStamp = {
        taskCount,
        this->taskLevel,
        flushStamp->peekStamp()};

    if (levelClosed) {
        this->taskLevel++;
    }

    return completionStamp;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programSamplerCacheFlushBetweenRedescribedSurfaceReads(LinearStream &commandStreamCSR) {
    if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
        PipeControlArgs args;
        args.textureCacheInvalidationEnable = true;
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);
        if (this->samplerCacheFlushRequired == SamplerCacheFlushState::samplerCacheFlushBefore) {
            this->samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushAfter;
        } else {
            this->samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
        }
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushPipelineSelectState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData) {
    if (flushData.pipelineSelectFullConfigurationNeeded) {
        this->streamProperties.pipelineSelect.copyPropertiesAll(dispatchFlags.requiredState->pipelineSelect);
        flushData.pipelineSelectDirty = true;
        setPreambleSetFlag(true);
    } else if (dispatchFlags.dispatchOperation == AppendOperations::kernel) {
        this->streamProperties.pipelineSelect.copyPropertiesSystolicMode(dispatchFlags.requiredState->pipelineSelect);
        flushData.pipelineSelectDirty = this->streamProperties.pipelineSelect.isDirty();
    }

    if (flushData.pipelineSelectDirty) {
        flushData.estimatedSize += PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(peekRootDeviceEnvironment());
    }

    flushData.pipelineSelectArgs = {
        this->streamProperties.pipelineSelect.systolicMode.value == 1,
        false,
        false,
        this->pipelineSupportFlags.systolicMode};
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushPipelineSelectCommand(ImmediateFlushData &flushData, LinearStream &csrStream) {
    if (flushData.pipelineSelectDirty) {
        PreambleHelper<GfxFamily>::programPipelineSelect(&csrStream, flushData.pipelineSelectArgs, peekRootDeviceEnvironment());
        this->streamProperties.pipelineSelect.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushFrontEndState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData) {
    if (flushData.frontEndFullConfigurationNeeded) {
        this->streamProperties.frontEndState.copyPropertiesAll(dispatchFlags.requiredState->frontEndState);
        flushData.frontEndDirty = true;
        setMediaVFEStateDirty(false);
    } else if (dispatchFlags.dispatchOperation == AppendOperations::kernel) {
        this->streamProperties.frontEndState.copyPropertiesComputeDispatchAllWalkerEnableDisableEuFusion(dispatchFlags.requiredState->frontEndState);
        flushData.frontEndDirty = this->streamProperties.frontEndState.isDirty();
    }

    if (flushData.frontEndDirty) {
        flushData.estimatedSize += NEO::PreambleHelper<GfxFamily>::getVFECommandsSize();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushFrontEndCommand(ImmediateFlushData &flushData, Device &device, LinearStream &csrStream) {
    if (flushData.frontEndDirty) {
        auto &gfxCoreHelper = getGfxCoreHelper();
        auto engineGroupType = gfxCoreHelper.getEngineGroupType(getOsContext().getEngineType(), getOsContext().getEngineUsage(), peekHwInfo());

        auto feStateCmdSpace = PreambleHelper<GfxFamily>::getSpaceForVfeState(&csrStream, peekHwInfo(), engineGroupType);
        PreambleHelper<GfxFamily>::programVfeState(feStateCmdSpace,
                                                   peekRootDeviceEnvironment(),
                                                   requiredScratchSlot0Size,
                                                   getScratchPatchAddress(),
                                                   device.getDeviceInfo().maxFrontEndThreads,
                                                   this->streamProperties);
        this->streamProperties.frontEndState.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushStateComputeModeState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData) {
    if (flushData.stateComputeModeFullConfigurationNeeded) {
        this->streamProperties.stateComputeMode.copyPropertiesAll(dispatchFlags.requiredState->stateComputeMode);
        flushData.stateComputeModeDirty = true;
        setStateComputeModeDirty(false);
    } else if (dispatchFlags.dispatchOperation == AppendOperations::kernel) {
        this->streamProperties.stateComputeMode.copyPropertiesGrfNumberThreadArbitration(dispatchFlags.requiredState->stateComputeMode);
        flushData.stateComputeModeDirty = this->streamProperties.stateComputeMode.isDirty();
    }

    if (flushData.stateComputeModeDirty) {
        flushData.estimatedSize += EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(peekRootDeviceEnvironment(), false, isRcs());
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushStateComputeModeCommand(ImmediateFlushData &flushData, LinearStream &csrStream) {
    if (flushData.stateComputeModeDirty) {
        EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(csrStream, this->streamProperties.stateComputeMode,
                                                                                   flushData.pipelineSelectArgs,
                                                                                   false, peekRootDeviceEnvironment(), isRcs(),
                                                                                   getDcFlushSupport());
        this->streamProperties.stateComputeMode.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushStateBaseAddressState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData, Device &device) {
    if (flushData.stateBaseAddressFullConfigurationNeeded) {
        this->streamProperties.stateBaseAddress.copyPropertiesAll(dispatchFlags.requiredState->stateBaseAddress);
        flushData.stateBaseAddressDirty = true;
        setGSBAStateDirty(false);
    } else if (dispatchFlags.dispatchOperation == AppendOperations::kernel) {
        if (this->streamProperties.stateBaseAddress.indirectObjectBaseAddress.value == StreamProperty64::initValue) {
            this->streamProperties.stateBaseAddress.copyPropertiesStatelessMocsIndirectState(dispatchFlags.requiredState->stateBaseAddress);
        } else {
            this->streamProperties.stateBaseAddress.copyPropertiesStatelessMocs(dispatchFlags.requiredState->stateBaseAddress);
        }
        if (globalStatelessHeapAllocation == nullptr) {
            this->streamProperties.stateBaseAddress.copyPropertiesBindingTableSurfaceState(dispatchFlags.requiredState->stateBaseAddress);
            if (this->dshSupported) {
                this->streamProperties.stateBaseAddress.copyPropertiesDynamicState(dispatchFlags.requiredState->stateBaseAddress);
            }
        } else {
            this->streamProperties.stateBaseAddress.copyPropertiesSurfaceState(dispatchFlags.requiredState->stateBaseAddress);
        }
        flushData.stateBaseAddressDirty = this->streamProperties.stateBaseAddress.isDirty();
    }

    if (flushData.stateBaseAddressDirty) {
        flushData.estimatedSize += getRequiredStateBaseAddressSize(device);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushStateBaseAddressCommand(ImmediateFlushData &flushData, LinearStream &csrStream, Device &device) {
    if (flushData.stateBaseAddressDirty) {
        bool btCommandNeeded = this->streamProperties.stateBaseAddress.bindingTablePoolBaseAddress.value != StreamProperty64::initValue;
        programStateBaseAddressCommon(nullptr, nullptr, nullptr, &this->streamProperties.stateBaseAddress,
                                      0, 0, flushData.pipelineSelectArgs, device, csrStream, btCommandNeeded, device.getNumGenericSubDevices() > 1, false);
        this->streamProperties.stateBaseAddress.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushOneTimeContextInitState(ImmediateDispatchFlags &dispatchFlags, ImmediateFlushData &flushData, Device &device) {
    size_t size = 0;
    size = getCmdSizeForPrologue();

    flushData.contextOneTimeInit = size > 0;
    flushData.estimatedSize += size;

    if (this->isProgramActivePartitionConfigRequired()) {
        flushData.contextOneTimeInit = true;
        flushData.estimatedSize += this->getCmdSizeForActivePartitionConfig();
    }

    if (this->isRayTracingStateProgramingNeeded(device)) {
        flushData.contextOneTimeInit = true;
        flushData.estimatedSize += this->getCmdSizeForPerDssBackedBuffer(peekHwInfo());
    }

    if (device.getDebugger() != nullptr) {
        if (!this->csrSurfaceProgrammed()) {
            flushData.contextOneTimeInit = true;
            flushData.estimatedSize += PreemptionHelper::getRequiredPreambleSize<GfxFamily>(device);
        }
        flushData.estimatedSize += this->getCmdSizeForExceptions();
    } else if (this->getPreemptionMode() == PreemptionMode::Initial) {
        flushData.contextOneTimeInit = true;
        flushData.estimatedSize += PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(device.getPreemptionMode(), this->getPreemptionMode());
        flushData.estimatedSize += PreemptionHelper::getRequiredPreambleSize<GfxFamily>(device);
    }

    if (!this->isStateSipSent) {
        size_t size = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(device, isRcs());

        flushData.contextOneTimeInit |= size > 0;
        flushData.estimatedSize += size;
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushOneTimeContextInitCommand(ImmediateFlushData &flushData, LinearStream &csrStream, Device &device) {
    if (flushData.contextOneTimeInit) {
        programEnginePrologue(csrStream);

        if (this->isProgramActivePartitionConfigRequired()) {
            this->programActivePartitionConfig(csrStream);
        }

        if (this->isRayTracingStateProgramingNeeded(device)) {
            this->dispatchRayTracingStateCommand(csrStream, device);
        }

        if (device.getDebugger() != nullptr) {
            PreemptionHelper::programCsrBaseAddress<GfxFamily>(csrStream,
                                                               device,
                                                               device.getDebugSurface());
            this->setCsrSurfaceProgrammed(true);
            this->programExceptions(csrStream, device);
        } else if (this->getPreemptionMode() == PreemptionMode::Initial) {
            PreemptionHelper::programCmdStream<GfxFamily>(csrStream, device.getPreemptionMode(), this->getPreemptionMode(), this->getPreemptionAllocation());
            PreemptionHelper::programCsrBaseAddress<GfxFamily>(csrStream,
                                                               device,
                                                               getPreemptionAllocation());
            this->setPreemptionMode(device.getPreemptionMode());
        }

        programStateSip(csrStream, device);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushAllocationsResidency(Device &device,
                                                                                  LinearStream &immediateCommandStream,
                                                                                  ImmediateFlushData &flushData,
                                                                                  LinearStream &csrStream) {
    this->makeResident(*tagAllocation);

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    if (getWorkPartitionAllocation()) {
        makeResident(*getWorkPartitionAllocation());
    }

    if (device.getRTMemoryBackedBuffer()) {
        makeResident(*device.getRTMemoryBackedBuffer());
    }

    if (flushData.estimatedSize > 0) {
        makeResident(*csrStream.getGraphicsAllocation());
    }

    if (getPreemptionAllocation()) {
        makeResident(*getPreemptionAllocation());
    }

    if (device.isStateSipRequired()) {
        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, this->osContext).getSipAllocation();
        makeResident(*sipAllocation);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushJumpToImmediate(ImmediateFlushData &flushData) {
    if (flushData.estimatedSize > 0) {
        flushData.estimatedSize += EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize();
        flushData.estimatedSize = alignUp(flushData.estimatedSize, MemoryConstants::cacheLineSize);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushJumpToImmediateCommand(LinearStream &immediateCommandStream,
                                                                                      size_t immediateCommandStreamStart,
                                                                                      ImmediateFlushData &flushData,
                                                                                      LinearStream &csrStream) {
    if (flushData.estimatedSize > 0) {
        uint64_t immediateStartAddress = immediateCommandStream.getGpuBase() + immediateCommandStreamStart;

        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&csrStream, immediateStartAddress, false, false, false);
        EncodeNoop<GfxFamily>::alignToCacheLine(csrStream);
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchImmediateFlushClientBufferCommands(ImmediateDispatchFlags &dispatchFlags,
                                                                                    LinearStream &immediateCommandStream,
                                                                                    ImmediateFlushData &flushData) {
    LinearStream &epilogueCommandStream = dispatchFlags.optionalEpilogueCmdStream != nullptr ? *dispatchFlags.optionalEpilogueCmdStream : immediateCommandStream;

    if (dispatchFlags.blockingAppend || dispatchFlags.requireTaskCountUpdate) {
        auto address = getTagAllocation()->getGpuAddress();

        PipeControlArgs args = {};
        args.dcFlushEnable = this->dcFlushSupport;
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        args.workloadPartitionOffset = isMultiTileOperationEnabled();
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            epilogueCommandStream,
            PostSyncMode::immediateData,
            address,
            this->taskCount + 1,
            peekRootDeviceEnvironment(),
            args);
    }

    makeResident(*immediateCommandStream.getGraphicsAllocation());
    if (dispatchFlags.optionalEpilogueCmdStream != nullptr) {
        makeResident(*dispatchFlags.optionalEpilogueCmdStream->getGraphicsAllocation());
    }

    programEndingCmd(epilogueCommandStream, &flushData.endPtr, isDirectSubmissionEnabled(), dispatchFlags.hasRelaxedOrderingDependencies, EngineHelpers::isBcs(this->osContext->getEngineType()));
    EncodeNoop<GfxFamily>::alignToCacheLine(epilogueCommandStream);
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushSendBatchBuffer(LinearStream &immediateCommandStream,
                                                                                        size_t immediateCommandStreamStart,
                                                                                        ImmediateDispatchFlags &dispatchFlags,
                                                                                        ImmediateFlushData &flushData,
                                                                                        LinearStream &csrStream) {
    this->latestSentTaskCount = taskCount + 1;

    bool startFromCsr = flushData.estimatedSize > 0;
    size_t startOffset = startFromCsr ? flushData.csrStartOffset : immediateCommandStreamStart;
    auto &streamToSubmit = startFromCsr ? csrStream : immediateCommandStream;
    GraphicsAllocation *chainedBatchBuffer = startFromCsr ? immediateCommandStream.getGraphicsAllocation() : nullptr;
    size_t chainedBatchBufferStartOffset = startFromCsr ? csrStream.getUsed() : 0;
    uint64_t taskStartAddress = immediateCommandStream.getGpuBase() + immediateCommandStreamStart;
    bool hasStallingCmds = (startFromCsr || dispatchFlags.blockingAppend || dispatchFlags.hasStallingCmds);
    bool dispatchMonitorFence = dispatchFlags.blockingAppend || dispatchFlags.requireTaskCountUpdate;

    constexpr bool immediateLowPriority = false;
    const QueueThrottle immediateThrottle = getThrottleFromPowerSavingUint(this->getUmdPowerHintValue());
    constexpr uint64_t immediateSliceCount = QueueSliceCount::defaultSliceCount;

    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, chainedBatchBufferStartOffset, taskStartAddress, chainedBatchBuffer,
                            immediateLowPriority, immediateThrottle, immediateSliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, flushData.endPtr, this->getNumClients(), hasStallingCmds,
                            dispatchFlags.hasRelaxedOrderingDependencies, dispatchMonitorFence, false};
    batchBuffer.disableFlatRingBuffer = dispatchFlags.dispatchOperation == AppendOperations::cmdList;

    updateStreamTaskCount(streamToSubmit, taskCount + 1);

    auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
    if (submissionStatus != SubmissionStatus::success) {
        --this->latestSentTaskCount;
        updateStreamTaskCount(streamToSubmit, taskCount);

        CompletionStamp completionStamp = {CompletionStamp::getTaskCountFromSubmissionStatusError(submissionStatus)};
        return completionStamp;
    } else {
        if (dispatchFlags.blockingAppend || dispatchFlags.requireTaskCountUpdate) {
            this->latestFlushedTaskCount = this->taskCount + 1;
        }

        ++taskCount;
        CompletionStamp completionStamp = {
            this->taskCount,
            this->taskLevel,
            flushStamp->peekStamp()};

        return completionStamp;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::handleBatchedDispatchImplicitFlush(uint64_t globalMemorySize, bool implicitFlush) {
    // check if we are not over the budget, if we are do implicit flush
    if (getMemoryManager()->isMemoryBudgetExhausted()) {
        if (this->totalMemoryUsed >= globalMemorySize / 4) {
            implicitFlush = true;
        }
    }

    if (debugManager.flags.PerformImplicitFlushEveryEnqueueCount.get() != -1) {
        if ((taskCount + 1) % debugManager.flags.PerformImplicitFlushEveryEnqueueCount.get() == 0) {
            implicitFlush = true;
        }
    }

    if (this->newResources) {
        implicitFlush = true;
        this->newResources = false;
    }
    implicitFlush |= checkImplicitFlushForGpuIdle();

    if (implicitFlush) {
        this->flushBatchedSubmissions();
    }
}

template <typename GfxFamily>
inline BatchBuffer CommandStreamReceiverHw<GfxFamily>::prepareBatchBufferForSubmission(LinearStream &commandStreamTask,
                                                                                       size_t commandStreamStartTask,
                                                                                       LinearStream &commandStreamCSR,
                                                                                       size_t commandStreamStartCSR,
                                                                                       DispatchFlags &dispatchFlags,
                                                                                       Device &device,
                                                                                       bool submitTask,
                                                                                       bool submitCSR,
                                                                                       bool hasStallingCmdsOnTaskStream) {

    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    bool submitCommandStreamFromCsr = false;
    bool directSubmissionEnabled = isDirectSubmissionEnabled();
    void *bbEndLocation = nullptr;
    size_t chainedBatchBufferStartOffset = 0;
    GraphicsAllocation *chainedBatchBuffer = nullptr;

    auto bbEndPaddingSize = this->dispatchMode == DispatchMode::immediateDispatch ? 0 : sizeof(MI_BATCH_BUFFER_START) - sizeof(MI_BATCH_BUFFER_END);

    // If the CSR has work in its CS, flush it before the task

    if (submitTask) {
        LinearStream &epilogueCommandStream = dispatchFlags.optionalEpilogueCmdStream != nullptr ? *dispatchFlags.optionalEpilogueCmdStream : commandStreamTask;
        programEndingCmd(epilogueCommandStream, &bbEndLocation, directSubmissionEnabled, dispatchFlags.hasRelaxedOrderingDependencies, EngineHelpers::isBcs(this->osContext->getEngineType()));
        EncodeNoop<GfxFamily>::emitNoop(epilogueCommandStream, bbEndPaddingSize);
        EncodeNoop<GfxFamily>::alignToCacheLine(epilogueCommandStream);

        if (submitCSR) {
            chainCsrWorkToTask(commandStreamCSR, commandStreamTask, commandStreamStartTask, bbEndLocation, chainedBatchBufferStartOffset, chainedBatchBuffer);
            submitCommandStreamFromCsr = true;
        } else if (dispatchFlags.epilogueRequired) {
            this->makeResident(*commandStreamCSR.getGraphicsAllocation());
        }
        this->programEpilogue(commandStreamCSR, device, &bbEndLocation, dispatchFlags);

    } else if (submitCSR) {
        programEndingCmd(commandStreamCSR, &bbEndLocation, directSubmissionEnabled, dispatchFlags.hasRelaxedOrderingDependencies, EngineHelpers::isBcs(this->osContext->getEngineType()));
        EncodeNoop<GfxFamily>::emitNoop(commandStreamCSR, bbEndPaddingSize);
        EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);
        DEBUG_BREAK_IF(commandStreamCSR.getUsed() > commandStreamCSR.getMaxAvailableSpace());
        submitCommandStreamFromCsr = true;
    }

    uint64_t taskStartAddress = commandStreamTask.getGpuBase() + commandStreamStartTask;
    size_t startOffset = submitCommandStreamFromCsr ? commandStreamStartCSR : commandStreamStartTask;
    auto &streamToSubmit = submitCommandStreamFromCsr ? commandStreamCSR : commandStreamTask;

    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, chainedBatchBufferStartOffset, taskStartAddress, chainedBatchBuffer,
                            dispatchFlags.lowPriority, dispatchFlags.throttle, dispatchFlags.sliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, bbEndLocation, this->getNumClients(), (submitCSR || dispatchFlags.hasStallingCmds || hasStallingCmdsOnTaskStream),
                            dispatchFlags.hasRelaxedOrderingDependencies, hasStallingCmdsOnTaskStream, false};

    updateStreamTaskCount(streamToSubmit, taskCount + 1);

    return batchBuffer;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::chainCsrWorkToTask(LinearStream &commandStreamCSR, LinearStream &commandStreamTask, size_t commandStreamStartTask, void *bbEndLocation, size_t &chainedBatchBufferStartOffset, GraphicsAllocation *&chainedBatchBuffer) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    chainedBatchBufferStartOffset = commandStreamCSR.getUsed();
    chainedBatchBuffer = commandStreamTask.getGraphicsAllocation();
    // Add MI_BATCH_BUFFER_START to chain from CSR -> Task
    auto pBBS = reinterpret_cast<MI_BATCH_BUFFER_START *>(commandStreamCSR.getSpace(sizeof(MI_BATCH_BUFFER_START)));
    addBatchBufferStart(pBBS, ptrOffset(commandStreamTask.getGraphicsAllocation()->getGpuAddress(), commandStreamStartTask), false);

    if (debugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        uint64_t baseCpu = reinterpret_cast<uint64_t>(commandStreamTask.getCpuBase());
        uint64_t baseGpu = commandStreamTask.getGraphicsAllocation()->getGpuAddress();
        uint64_t startOffset = commandStreamStartTask;
        uint64_t endOffset = sizeof(MI_BATCH_BUFFER_START) +
                             static_cast<uint64_t>(ptrDiff(bbEndLocation, commandStreamTask.getGraphicsAllocation()->getGpuAddress()));

        flatBatchBufferHelper->registerCommandChunk(baseCpu, baseGpu, startOffset, endOffset);
    }

    DEBUG_BREAK_IF(chainedBatchBuffer == nullptr);
    this->makeResident(*chainedBatchBuffer);
    EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);
}
template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::submitDependencyUpdate(TagNodeBase *tag) {
    if (tag == nullptr) {
        return false;
    }
    auto ownership = obtainUniqueOwnership();
    PipeControlArgs args;
    auto expectedSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), NEO::PostSyncMode::immediateData) + this->getCmdSizeForPrologue();
    auto &commandStream = getCS(expectedSize);
    auto commandStreamStart = commandStream.getUsed();
    auto cacheFlushTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*tag);
    this->programEnginePrologue(commandStream);
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, this->peekRootDeviceEnvironment());
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        commandStream,
        PostSyncMode::immediateData,
        cacheFlushTimestampPacketGpuAddress,
        0,
        this->peekRootDeviceEnvironment(),
        args);
    makeResident(*(tag->getBaseGraphicsAllocation()->getDefaultGraphicsAllocation()));
    auto submissionStatus = this->flushSmallTask(commandStream, commandStreamStart);
    return submissionStatus == SubmissionStatus::success;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::unblockPagingFenceSemaphore(uint64_t pagingFenceValue) {
    if (this->isAnyDirectSubmissionEnabled()) {
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            this->blitterDirectSubmission->unblockPagingFenceSemaphore(pagingFenceValue);
        } else {
            this->directSubmission->unblockPagingFenceSemaphore(pagingFenceValue);
        }
    }
}

} // namespace NEO
