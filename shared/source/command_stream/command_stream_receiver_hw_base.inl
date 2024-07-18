/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
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
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/blit_properties.h"
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
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/tag_allocator.h"

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

    if (DebugManager.flags.FlattenBatchBufferForAUBDump.get() || DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        flatBatchBufferHelper.reset(new FlatBatchBufferHelperHw<GfxFamily>(executionEnvironment));
    }
    defaultSshSize = HeapSize::getDefaultHeapSize(EncodeStates<GfxFamily>::getSshHeapSize());
    canUse4GbHeaps = are4GbHeapsAvailable();

    timestampPacketWriteEnabled = gfxCoreHelper.timestampPacketWriteSupported();
    if (DebugManager.flags.EnableTimestampPacket.get() != -1) {
        timestampPacketWriteEnabled = !!DebugManager.flags.EnableTimestampPacket.get();
    }

    createScratchSpaceController();
    configurePostSyncWriteOffset();

    this->dcFlushSupport = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]);
    this->dshSupported = hwInfo.capabilityTable.supportsImages;
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    return SubmissionStatus::SUCCESS;
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
                                                                 bool hasRelaxedOrderingDependencies) {
    if (directSubmissionEnabled) {
        uint64_t startAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed();
        if (DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
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
            NEO::EncodeSetMMIO<GfxFamily>::encodeREG(commandStream, CS_GPR_R0, CS_GPR_R3);
            NEO::EncodeSetMMIO<GfxFamily>::encodeREG(commandStream, CS_GPR_R0 + 4, CS_GPR_R3 + 4);

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
    if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
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
        if (DebugManager.flags.ForceSemaphoreDelayBetweenWaits.get() > -1) {
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
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushBcsTask(LinearStream &commandStreamTask, size_t commandStreamTaskStart,
                                                                 const DispatchBcsFlags &dispatchBcsFlags, const HardwareInfo &hwInfo) {
    UNRECOVERABLE_IF(this->dispatchMode != DispatchMode::ImmediateDispatch);

    uint64_t taskStartAddress = commandStreamTask.getGpuBase() + commandStreamTaskStart;

    if (dispatchBcsFlags.flushTaskCount) {
        uint64_t postSyncAddress = getTagAllocation()->getGpuAddress();
        TaskCountType postSyncData = peekTaskCount() + 1;
        NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&(this->peekRootDeviceEnvironment()))};
        NEO::MiFlushArgs args{waArgs};
        args.commandWithPostSync = true;
        args.notifyEnable = isUsedNotifyEnableForPostSync();

        NEO::EncodeMiFlushDW<GfxFamily>::programWithWa(commandStreamTask, postSyncAddress, postSyncData, args);
    }

    auto &commandStreamCSR = getCS(getRequiredCmdStreamSizeAligned(dispatchBcsFlags));
    size_t commandStreamStartCSR = commandStreamCSR.getUsed();

    programHardwareContext(commandStreamCSR);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    if (dispatchBcsFlags.flushTaskCount) {
        makeResident(*getTagAllocation());
    }

    makeResident(*commandStreamTask.getGraphicsAllocation());

    bool submitCSR = (commandStreamStartCSR != commandStreamCSR.getUsed());
    void *bbEndLocation = nullptr;

    programEndingCmd(commandStreamTask, &bbEndLocation, isBlitterDirectSubmissionEnabled(), dispatchBcsFlags.hasRelaxedOrderingDependencies);
    EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamTask);

    if (submitCSR) {
        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(commandStreamCSR.getSpace(sizeof(MI_BATCH_BUFFER_START)));
        addBatchBufferStart(bbStart, taskStartAddress, false);
        EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);

        this->makeResident(*commandStreamCSR.getGraphicsAllocation());
    }

    size_t startOffset = submitCSR ? commandStreamStartCSR : commandStreamTaskStart;
    auto &streamToSubmit = submitCSR ? commandStreamCSR : commandStreamTask;

    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, 0, taskStartAddress, nullptr,
                            false, QueueThrottle::MEDIUM, NEO::QueueSliceCount::defaultSliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, bbEndLocation, this->getNumClients(), (submitCSR || dispatchBcsFlags.hasStallingCmds),
                            dispatchBcsFlags.hasRelaxedOrderingDependencies, dispatchBcsFlags.flushTaskCount};

    updateStreamTaskCount(streamToSubmit, taskCount + 1);
    this->latestSentTaskCount = taskCount + 1;

    auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
    if (submissionStatus != SubmissionStatus::SUCCESS) {
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
    flushData.pipelineSelectFullConfigurationNeeded = !getPreambleSetFlag();
    flushData.frontEndFullConfigurationNeeded = getMediaVFEStateDirty();
    flushData.stateComputeModeFullConfigurationNeeded = getStateComputeModeDirty();
    flushData.stateBaseAddressFullConfigurationNeeded = getGSBAStateDirty();

    if (dispatchFlags.sshCpuBase != nullptr && (this->requiredScratchSize > 0 || this->requiredPrivateScratchSize > 0)) {
        bool checkFeStateDirty = false;
        bool checkSbaStateDirty = false;
        scratchSpaceController->setRequiredScratchSpace(dispatchFlags.sshCpuBase,
                                                        0u,
                                                        this->requiredScratchSize,
                                                        this->requiredPrivateScratchSize,
                                                        this->taskCount,
                                                        *this->osContext,
                                                        checkSbaStateDirty,
                                                        checkFeStateDirty);
        flushData.frontEndFullConfigurationNeeded |= checkFeStateDirty;
        flushData.stateBaseAddressFullConfigurationNeeded |= checkSbaStateDirty;

        if (scratchSpaceController->getScratchSpaceAllocation()) {
            makeResident(*scratchSpaceController->getScratchSpaceAllocation());
        }
        if (scratchSpaceController->getPrivateScratchSpaceAllocation()) {
            makeResident(*scratchSpaceController->getPrivateScratchSpaceAllocation());
        }
    }

    handleImmediateFlushPipelineSelectState(dispatchFlags, flushData);
    handleImmediateFlushFrontEndState(dispatchFlags, flushData);
    handleImmediateFlushStateComputeModeState(dispatchFlags, flushData);
    handleImmediateFlushStateBaseAddressState(dispatchFlags, flushData, device);
    handleImmediateFlushOneTimeContextInitState(dispatchFlags, flushData, device);

    handleImmediateFlushJumpToImmediate(flushData);

    bool stateCacheFlushRequired = device.getBindlessHeapsHelper() ? device.getBindlessHeapsHelper()->getStateDirtyForContext(getOsContext().getContextId()) : false;
    if (stateCacheFlushRequired) {
        flushData.estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush();
    }

    auto &csrCommandStream = getCS(flushData.estimatedSize);
    flushData.csrStartOffset = csrCommandStream.getUsed();

    if (stateCacheFlushRequired) {
        device.getBindlessHeapsHelper()->clearStateDirtyForContext(getOsContext().getContextId());
        MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(csrCommandStream, device.getRootDeviceEnvironment());
    }

    dispatchImmediateFlushPipelineSelectCommand(flushData, csrCommandStream);
    dispatchImmediateFlushFrontEndCommand(flushData, device, csrCommandStream);
    dispatchImmediateFlushStateComputeModeCommand(flushData, csrCommandStream);
    dispatchImmediateFlushStateBaseAddressCommand(flushData, csrCommandStream, device);
    dispatchImmediateFlushOneTimeContextInitCommand(flushData, csrCommandStream, device);

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
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    auto &rootDeviceEnvironment = this->peekRootDeviceEnvironment();

    DEBUG_BREAK_IF(&commandStreamTask == &commandStream);
    DEBUG_BREAK_IF(!(dispatchFlags.preemptionMode == PreemptionMode::Disabled ? device.getPreemptionMode() == PreemptionMode::Disabled : true));
    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskLevel", taskLevel);

    auto levelClosed = false;
    bool implicitFlush = dispatchFlags.implicitFlush || dispatchFlags.blocking || DebugManager.flags.ForceImplicitFlush.get();
    void *currentPipeControlForNooping = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    PipeControlArgs args;

    if (DebugManager.flags.ForceCsrFlushing.get()) {
        flushBatchedSubmissions();
    }

    if (detectInitProgrammingFlagsRequired(dispatchFlags)) {
        initProgrammingFlags();
    }

    const auto &hwInfo = peekHwInfo();

    bool hasStallingCmdsOnTaskStream = false;

    if (dispatchFlags.blocking || dispatchFlags.dcFlush || dispatchFlags.guardCommandBufferWithPipeControl || this->heapStorageRequiresRecyclingTag) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            // for ImmediateDispatch we will send this right away, therefore this pipe control will close the level
            // for BatchedSubmissions it will be nooped and only last ppc in batch will be emitted.
            levelClosed = true;
            // if we guard with ppc, flush dc as well to speed up completion latency
            if (dispatchFlags.guardCommandBufferWithPipeControl || this->heapStorageRequiresRecyclingTag || dispatchFlags.blocking) {
                dispatchFlags.dcFlush = this->dcFlushSupport;
            }
        }

        this->heapStorageRequiresRecyclingTag = false;
        epiloguePipeControlLocation = ptrOffset(commandStreamTask.getCpuBase(), commandStreamTask.getUsed());

        if ((dispatchFlags.outOfOrderExecutionAllowed || timestampPacketWriteEnabled) &&
            !dispatchFlags.dcFlush) {
            currentPipeControlForNooping = epiloguePipeControlLocation;
        }

        hasStallingCmdsOnTaskStream = true;

        auto address = getTagAllocation()->getGpuAddress();

        args.dcFlushEnable = getDcFlushRequired(dispatchFlags.dcFlush);
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        args.tlbInvalidation |= dispatchFlags.memoryMigrationRequired;
        args.textureCacheInvalidationEnable |= dispatchFlags.textureCacheFlush;
        args.workloadPartitionOffset = isMultiTileOperationEnabled();
        args.stateCacheInvalidationEnable = dispatchFlags.stateCacheInvalidation;
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            commandStreamTask,
            PostSyncMode::ImmediateData,
            address,
            taskCount + 1,
            rootDeviceEnvironment,
            args);

        DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", peekTaskCount());
        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            flatBatchBufferHelper->setPatchInfoData(PatchInfoData(address, 0u,
                                                                  PatchInfoAllocationType::TagAddress,
                                                                  commandStreamTask.getGraphicsAllocation()->getGpuAddress(),
                                                                  commandStreamTask.getUsed() - 2 * sizeof(uint64_t),
                                                                  PatchInfoAllocationType::Default));
            flatBatchBufferHelper->setPatchInfoData(PatchInfoData(address, 0u,
                                                                  PatchInfoAllocationType::TagValue,
                                                                  commandStreamTask.getGraphicsAllocation()->getGpuAddress(),
                                                                  commandStreamTask.getUsed() - sizeof(uint64_t),
                                                                  PatchInfoAllocationType::Default));
        }
    }
    this->latestSentTaskCount = taskCount + 1;

    if (DebugManager.flags.ForceSLML3Config.get()) {
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
    if (ssh && (requiredScratchSize || requiredPrivateScratchSize)) {
        scratchSpaceController->setRequiredScratchSpace(ssh->getCpuBase(),
                                                        0u,
                                                        requiredScratchSize,
                                                        requiredPrivateScratchSize,
                                                        this->taskCount,
                                                        *this->osContext,
                                                        stateBaseAddressDirty,
                                                        checkVfeStateDirty);
        if (checkVfeStateDirty) {
            setMediaVFEStateDirty(true);
        }
        if (scratchSpaceController->getScratchSpaceAllocation()) {
            makeResident(*scratchSpaceController->getScratchSpaceAllocation());
        }
        if (scratchSpaceController->getPrivateScratchSpaceAllocation()) {
            makeResident(*scratchSpaceController->getPrivateScratchSpaceAllocation());
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
        estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush();
    }
    auto &commandStreamCSR = this->getCS(estimatedSize);
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies, false);
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);

    programActivePartitionConfigFlushTask(commandStreamCSR);
    programEngineModeCommands(commandStreamCSR, dispatchFlags);

    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = pageTableManager->initPageTableManagerRegisters(this);
    }

    programHardwareContext(commandStreamCSR);
    programPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs);
    programComputeMode(commandStreamCSR, dispatchFlags, hwInfo);
    programL3(commandStreamCSR, newL3Config);
    programPreamble(commandStreamCSR, device, newL3Config);
    programMediaSampler(commandStreamCSR, dispatchFlags);
    addPipeControlBefore3dState(commandStreamCSR, dispatchFlags);
    programPerDssBackedBuffer(commandStreamCSR, device, dispatchFlags);
    if (isRayTracingStateProgramingNeeded(device)) {
        dispatchRayTracingStateCommand(commandStreamCSR, device);
    }

    programVFEState(commandStreamCSR, dispatchFlags, device.getDeviceInfo().maxFrontEndThreads);

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

    if (experimentalCmdBuffer.get() != nullptr) {
        size_t startingOffset = experimentalCmdBuffer->programExperimentalCommandBuffer<GfxFamily>();
        experimentalCmdBuffer->injectBufferStart<GfxFamily>(commandStreamCSR, startingOffset);
    }

    if (stateCacheFlushRequired) {
        device.getBindlessHeapsHelper()->clearStateDirtyForContext(getOsContext().getContextId());
        MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(commandStreamCSR, device.getRootDeviceEnvironment());
    }

    if (requiresInstructionCacheFlush) {
        PipeControlArgs args;
        args.instructionCacheInvalidateEnable = true;
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);
        requiresInstructionCacheFlush = false;
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

    if (DebugManager.flags.ForcePipeControlPriorToWalker.get()) {
        forcePipeControl(commandStreamCSR);
    }

    this->makeResident(*tagAllocation);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    if (preemptionAllocation) {
        makeResident(*preemptionAllocation);
    }

    bool debuggingEnabled = device.getDebugger() != nullptr;

    if (dispatchFlags.preemptionMode == PreemptionMode::MidThread || debuggingEnabled) {

        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, this->osContext).getSipAllocation();
        makeResident(*sipAllocation);
    }

    if (debuggingEnabled && debugSurface) {
        makeResident(*debugSurface);
    }

    if (experimentalCmdBuffer.get() != nullptr) {
        experimentalCmdBuffer->makeResidentAllocations();
    }

    if (workPartitionAllocation) {
        makeResident(*workPartitionAllocation);
    }

    auto rtBuffer = device.getRTMemoryBackedBuffer();
    if (rtBuffer) {
        makeResident(*rtBuffer);
    }

    // If the CSR has work in its CS, flush it before the task
    bool submitTask = commandStreamStartTask != commandStreamTask.getUsed();
    bool submitCSR = (commandStreamStartCSR != commandStreamCSR.getUsed());
    bool submitCommandStreamFromCsr = false;
    void *bbEndLocation = nullptr;
    auto bbEndPaddingSize = this->dispatchMode == DispatchMode::ImmediateDispatch ? 0 : sizeof(MI_BATCH_BUFFER_START) - sizeof(MI_BATCH_BUFFER_END);
    size_t chainedBatchBufferStartOffset = 0;
    GraphicsAllocation *chainedBatchBuffer = nullptr;
    bool directSubmissionEnabled = isDirectSubmissionEnabled();
    if (submitTask) {
        programEndingCmd(commandStreamTask, &bbEndLocation, directSubmissionEnabled, dispatchFlags.hasRelaxedOrderingDependencies);
        EncodeNoop<GfxFamily>::emitNoop(commandStreamTask, bbEndPaddingSize);
        EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamTask);

        if (submitCSR) {
            chainedBatchBufferStartOffset = commandStreamCSR.getUsed();
            chainedBatchBuffer = commandStreamTask.getGraphicsAllocation();
            // Add MI_BATCH_BUFFER_START to chain from CSR -> Task
            auto pBBS = reinterpret_cast<MI_BATCH_BUFFER_START *>(commandStreamCSR.getSpace(sizeof(MI_BATCH_BUFFER_START)));
            addBatchBufferStart(pBBS, ptrOffset(commandStreamTask.getGraphicsAllocation()->getGpuAddress(), commandStreamStartTask), false);
            if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                flatBatchBufferHelper->registerCommandChunk(commandStreamTask.getGraphicsAllocation()->getGpuAddress(),
                                                            reinterpret_cast<uint64_t>(commandStreamTask.getCpuBase()),
                                                            commandStreamStartTask,
                                                            static_cast<uint64_t>(ptrDiff(bbEndLocation,
                                                                                          commandStreamTask.getGraphicsAllocation()->getGpuAddress())) +
                                                                sizeof(MI_BATCH_BUFFER_START));
            }

            auto commandStreamAllocation = commandStreamTask.getGraphicsAllocation();
            DEBUG_BREAK_IF(commandStreamAllocation == nullptr);

            this->makeResident(*commandStreamAllocation);
            EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);
            submitCommandStreamFromCsr = true;
        } else if (dispatchFlags.epilogueRequired) {
            this->makeResident(*commandStreamCSR.getGraphicsAllocation());
        }
        this->programEpilogue(commandStreamCSR, device, &bbEndLocation, dispatchFlags);

    } else if (submitCSR) {
        programEndingCmd(commandStreamCSR, &bbEndLocation, directSubmissionEnabled, dispatchFlags.hasRelaxedOrderingDependencies);
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
                            dispatchFlags.hasRelaxedOrderingDependencies, hasStallingCmdsOnTaskStream};

    updateStreamTaskCount(streamToSubmit, taskCount + 1);

    if (submitCSR || submitTask) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
            if (submissionStatus != SubmissionStatus::SUCCESS) {
                updateStreamTaskCount(streamToSubmit, taskCount);
                CompletionStamp completionStamp = {CompletionStamp::getTaskCountFromSubmissionStatusError(submissionStatus)};
                return completionStamp;
            }
            if (hasStallingCmdsOnTaskStream) {
                this->latestFlushedTaskCount = this->taskCount + 1;
            }
        } else {
            auto commandBuffer = new CommandBuffer(device);
            commandBuffer->batchBuffer = batchBuffer;
            commandBuffer->surfaces.swap(this->getResidencyAllocations());
            commandBuffer->batchBufferEndLocation = bbEndLocation;
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

    if (this->dispatchMode == DispatchMode::BatchedDispatch) {
        // check if we are not over the budget, if we are do implicit flush
        if (getMemoryManager()->isMemoryBudgetExhausted()) {
            if (this->totalMemoryUsed >= device.getDeviceInfo().globalMemSize / 4) {
                implicitFlush = true;
            }
        }

        if (DebugManager.flags.PerformImplicitFlushEveryEnqueueCount.get() != -1) {
            if ((taskCount + 1) % DebugManager.flags.PerformImplicitFlushEveryEnqueueCount.get() == 0) {
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
    if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
        return true;
    }
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
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

            auto pipeControlLocationSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), lastPipeControlArgs.tlbInvalidation);

            FlushStampUpdateHelper flushStampUpdateHelper;
            flushStampUpdateHelper.insert(primaryCmdBuffer->flushStamp->getStampReference());

            currentPipeControlForNooping = primaryCmdBuffer->pipeControlThatMayBeErasedLocation;
            epiloguePipeControlLocation = primaryCmdBuffer->epiloguePipeControlLocation;

            if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                flatBatchBufferHelper->registerCommandChunk(primaryCmdBuffer->batchBuffer, sizeof(MI_BATCH_BUFFER_START));
            }

            while (nextCommandBuffer && nextCommandBuffer->inspectionId == primaryCmdBuffer->inspectionId) {

                // noop pipe control
                if (currentPipeControlForNooping) {
                    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
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

                if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
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

                if (DebugManager.flags.DisableDcFlushInEpilogue.get()) {
                    lastPipeControlArgs.dcFlushEnable = false;
                }

                MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                    epiloguePipeControlLocation,
                    PostSyncMode::ImmediateData,
                    getTagAllocation()->getGpuAddress(),
                    lastTaskCount,
                    peekRootDeviceEnvironment(),
                    lastPipeControlArgs);
            }

            primaryCmdBuffer->batchBuffer.endCmdPtr = currentBBendLocation;

            if (this->flush(primaryCmdBuffer->batchBuffer, surfacesForSubmit) != SubmissionStatus::SUCCESS) {
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
    return getCmdsSizeForHardwareContext() + sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);
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
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
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
    if (experimentalCmdBuffer.get() != nullptr) {
        size += experimentalCmdBuffer->getRequiredInjectionSize<GfxFamily>();
    }

    size += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.csrDependencies, false);
    size += TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<GfxFamily>(dispatchFlags.csrDependencies);

    if (dispatchFlags.isStallingCommandsOnNextFlushRequired) {
        size += getCmdSizeForStallingCommands(dispatchFlags);
    }

    if (requiresInstructionCacheFlush) {
        size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
    }

    if (DebugManager.flags.ForcePipeControlPriorToWalker.get()) {
        size += 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(false);
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
    if (status == WaitStatus::NotReady) {
        waitForFlushStamp(flushStampToWait);
        // now call blocking wait, this is to ensure that task count is reached
        status = waitForCompletionWithTimeout(WaitParams{false, false, 0}, taskCountToWait);
    }

    // If GPU hang occured, then propagate it to the caller.
    if (status == WaitStatus::GpuHang) {
        return status;
    }

    for (uint32_t i = 0; i < this->activePartitions; i++) {
        UNRECOVERABLE_IF(*(ptrOffset(getTagAddress(), (i * this->immWritePostSyncWriteOffset))) < taskCountToWait);
    }

    if (kmdNotifyHelper->quickKmdSleepForSporadicWaitsEnabled()) {
        kmdNotifyHelper->updateLastWaitForCompletionTimestamp();
    }
    return WaitStatus::Ready;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programPreemption(LinearStream &csr, DispatchFlags &dispatchFlags) {
    PreemptionHelper::programCmdStream<GfxFamily>(csr, dispatchFlags.preemptionMode, this->lastPreemptionMode, preemptionAllocation);
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
        PreambleHelper<GfxFamily>::programPreamble(&csr, device, newL3Config, this->preemptionAllocation);
        this->isPreambleSent = true;
        this->lastSentL3Config = newL3Config;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads) {
    if (mediaVfeStateDirty) {
        if (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotApplicable) {
            lastAdditionalKernelExecInfo = dispatchFlags.additionalKernelExecInfo;
        }
        if (dispatchFlags.kernelExecutionType != KernelExecutionType::NotApplicable) {
            lastKernelExecutionType = dispatchFlags.kernelExecutionType;
        }
        auto &hwInfo = peekHwInfo();

        auto isCooperative = dispatchFlags.kernelExecutionType == KernelExecutionType::Concurrent;
        auto disableOverdispatch = (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotSet);
        this->streamProperties.frontEndState.setPropertiesAll(isCooperative, dispatchFlags.disableEUFusion, disableOverdispatch, osContext->isEngineInstanced());

        auto &gfxCoreHelper = getGfxCoreHelper();
        auto engineGroupType = gfxCoreHelper.getEngineGroupType(getOsContext().getEngineType(), getOsContext().getEngineUsage(), hwInfo);
        auto pVfeState = PreambleHelper<GfxFamily>::getSpaceForVfeState(&csr, hwInfo, engineGroupType);
        PreambleHelper<GfxFamily>::programVfeState(
            pVfeState, peekRootDeviceEnvironment(), requiredScratchSize, getScratchPatchAddress(),
            maxFrontEndThreads, streamProperties);
        auto commandOffset = PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(&csr, pVfeState);

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
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
        PatchInfoData dynamicStatePatchInfo = {dsh->getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::DynamicStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
        flatBatchBufferHelper->setPatchInfoData(dynamicStatePatchInfo);
    }
    PatchInfoData generalStatePatchInfo = {generalStateBase, 0u, PatchInfoAllocationType::GeneralStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
    PatchInfoData surfaceStatePatchInfo = {ssh->getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::SurfaceStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};

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
void CommandStreamReceiverHw<GfxFamily>::setClearSlmWorkAroundParameter(PipeControlArgs &args) {
}

template <typename GfxFamily>
uint64_t CommandStreamReceiverHw<GfxFamily>::getScratchPatchAddress() {
    return scratchSpaceController->getScratchPatchAddress();
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::detectInitProgrammingFlagsRequired(const DispatchFlags &dispatchFlags) const {
    return DebugManager.flags.ForceCsrReprogramming.get();
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
    return directSubmissionRelaxedOrderingEnabled() && (DebugManager.flags.DirectSubmissionRelaxedOrderingForBcs.get() == 1) &&
           (blitPropertiesContainer.size() == 1) && !hasStallingCmds;
}

template <typename GfxFamily>
TaskCountType CommandStreamReceiverHw<GfxFamily>::flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto lock = obtainUniqueOwnership();
    bool blitterDirectSubmission = this->isBlitterDirectSubmissionEnabled();
    auto debugPauseEnabled = PauseOnGpuProperties::featureEnabled(DebugManager.flags.PauseOnBlitCopy.get());
    auto &rootDeviceEnvironment = this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex];

    const bool updateTag = !isUpdateTagFromWaitEnabled() || blocking;
    const bool hasStallingCmds = updateTag || !this->isEnginePrologueSent;
    const bool relaxedOrderingAllowed = bcsRelaxedOrderingAllowed(blitPropertiesContainer, hasStallingCmds);

    auto estimatedCsSize = BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitPropertiesContainer, profilingEnabled, debugPauseEnabled, blitterDirectSubmission,
                                                                                   relaxedOrderingAllowed, *rootDeviceEnvironment.get());
    auto &commandStream = getCS(estimatedCsSize);

    auto commandStreamStart = commandStream.getUsed();
    auto newTaskCount = taskCount + 1;
    latestSentTaskCount = newTaskCount;

    this->initializeResources();
    this->initDirectSubmission();

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
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
        RelaxedOrderingHelper::encodeRegistersBeforeDependencyCheckers<GfxFamily>(commandStream);
    }

    NEO::EncodeDummyBlitWaArgs waArgs{false, rootDeviceEnvironment.get()};
    MiFlushArgs args{waArgs};

    for (auto &blitProperties : blitPropertiesContainer) {
        TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, blitProperties.csrDependencies, isRelaxedOrderingDispatch);
        TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);

        BlitCommandsHelper<GfxFamily>::encodeWa(commandStream, blitProperties, latestSentBcsWaValue);

        if (blitProperties.outputTimestampPacket && profilingEnabled) {
            BlitCommandsHelper<GfxFamily>::encodeProfilingStartMmios(commandStream, *blitProperties.outputTimestampPacket);
        }

        BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(blitProperties, commandStream, waArgs);
        auto dummyAllocation = rootDeviceEnvironment->getDummyAllocation();
        if (dummyAllocation) {
            makeResident(*dummyAllocation);
        }

        if (blitProperties.outputTimestampPacket) {
            if (profilingEnabled) {
                EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, 0llu, newTaskCount, args);
                BlitCommandsHelper<GfxFamily>::encodeProfilingEndMmios(commandStream, *blitProperties.outputTimestampPacket);
            } else {
                auto timestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*blitProperties.outputTimestampPacket);
                args.commandWithPostSync = true;

                EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, timestampPacketGpuAddress, 0, args);
            }
            makeResident(*blitProperties.outputTimestampPacket->getBaseGraphicsAllocation());
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
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), false, peekRootDeviceEnvironment());
        args.commandWithPostSync = true;
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        EncodeMiFlushDW<GfxFamily>::programWithWa(commandStream, tagAllocation->getGpuAddress(), newTaskCount, args);

        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), false, peekRootDeviceEnvironment());
    }
    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(),
                                                                  DebugPauseState::waitingForUserEndConfirmation,
                                                                  DebugPauseState::hasUserEndConfirmation, *rootDeviceEnvironment.get());
    }

    void *endingCmdPtr = nullptr;
    programEndingCmd(commandStream, &endingCmdPtr, blitterDirectSubmission, isRelaxedOrderingDispatch);

    EncodeNoop<GfxFamily>::alignToCacheLine(commandStream);

    makeResident(*tagAllocation);
    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    uint64_t taskStartAddress = commandStream.getGpuBase() + commandStreamStart;

    BatchBuffer batchBuffer{commandStream.getGraphicsAllocation(), commandStreamStart, 0, taskStartAddress, nullptr, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStream.getUsed(), &commandStream, endingCmdPtr, this->getNumClients(), hasStallingCmds, isRelaxedOrderingDispatch, blocking};

    updateStreamTaskCount(commandStream, newTaskCount);

    auto flushSubmissionStatus = flush(batchBuffer, getResidencyAllocations());
    if (flushSubmissionStatus != SubmissionStatus::SUCCESS) {
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
        const auto waitStatus = waitForTaskCountWithKmdNotifyFallback(newTaskCount, flushStampToWait, false, QueueThrottle::MEDIUM);
        internalAllocationStorage->cleanAllocationList(newTaskCount, TEMPORARY_ALLOCATION);

        if (waitStatus == WaitStatus::GpuHang) {
            return CompletionStamp::gpuHang;
        }
    }

    return newTaskCount;
}

template <typename GfxFamily>
inline SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushTagUpdate() {
    if (this->osContext != nullptr) {
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            return this->flushMiFlushDW();
        } else {
            return this->flushPipeControl(false);
        }
    }
    return SubmissionStatus::DEVICE_UNINITIALIZED;
}

template <typename GfxFamily>
inline SubmissionStatus CommandStreamReceiverHw<GfxFamily>::flushMiFlushDW() {
    auto lock = obtainUniqueOwnership();

    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&peekRootDeviceEnvironment())};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = isUsedNotifyEnableForPostSync();

    auto &commandStream = getCS(EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs));
    auto commandStreamStart = commandStream.getUsed();

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
    }

    auto dispatchSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), args.tlbInvalidation) + this->getCmdSizeForPrologue();

    auto &commandStream = getCS(dispatchSize);
    auto commandStreamStart = commandStream.getUsed();

    this->programEnginePrologue(commandStream);

    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(commandStream,
                                                                              PostSyncMode::ImmediateData,
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
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    void *endingCmdPtr = nullptr;
    programEndingCmd(commandStreamTask, &endingCmdPtr, isAnyDirectSubmissionEnabled(), false);

    auto bytesToPad = EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferStartSize() -
                      EncodeBatchBufferStartOrEnd<GfxFamily>::getBatchBufferEndSize();
    EncodeNoop<GfxFamily>::emitNoop(commandStreamTask, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamTask);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    uint64_t taskStartAddress = commandStreamTask.getGpuBase() + commandStreamStartTask;

    BatchBuffer batchBuffer{commandStreamTask.getGraphicsAllocation(), commandStreamStartTask, 0, taskStartAddress,
                            nullptr, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStreamTask.getUsed(), &commandStreamTask, endingCmdPtr, this->getNumClients(), true, false, true};

    this->latestSentTaskCount = taskCount + 1;
    auto submissionStatus = flushHandler(batchBuffer, getResidencyAllocations());
    if (submissionStatus == SubmissionStatus::SUCCESS) {
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
    auto status = flush(batchBuffer, allocationsForResidency);
    makeSurfacePackNonResident(allocationsForResidency, true);
    return status;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isUpdateTagFromWaitEnabled() {
    auto &gfxCoreHelper = getGfxCoreHelper();
    auto enabled = gfxCoreHelper.isUpdateTaskCountFromWaitSupported();
    enabled &= this->isAnyDirectSubmissionEnabled();

    switch (DebugManager.flags.UpdateTaskCountFromWait.get()) {
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
    return MemoryCompressionState::NotApplicable;
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
        programEndingCmd(csr, batchBufferEndLocation, isDirectSubmissionEnabled(), false);
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
inline void CommandStreamReceiverHw<GfxFamily>::stopDirectSubmission(bool blocking) {
    if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
        this->blitterDirectSubmission->stopRingBuffer(blocking);
    } else {
        this->directSubmission->stopRingBuffer(blocking);
    }
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
                    ret = blitterDirectSubmission->initialize(submitOnInit, this->isUsedNotifyEnableForPostSync());
                    completionFenceValuePointer = blitterDirectSubmission->getCompletionValuePointer();

                } else {
                    directSubmission = DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::create(*this);
                    ret = directSubmission->initialize(submitOnInit, this->isUsedNotifyEnableForPostSync());
                    completionFenceValuePointer = directSubmission->getCompletionValuePointer();
                }
                auto directSubmissionController = executionEnvironment.initializeDirectSubmissionController();
                if (directSubmissionController) {
                    directSubmissionController->registerDirectSubmission(this);
                }
                if (this->isUpdateTagFromWaitEnabled()) {
                    this->overrideDispatchPolicy(DispatchMode::ImmediateDispatch);
                }
            }
        }
        this->osContext->setDirectSubmissionActive();
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
std::unique_ptr<TagAllocatorBase> CommandStreamReceiverHw<GfxFamily>::createMultiRootDeviceTimestampPacketAllocator(const RootDeviceIndicesContainer rootDeviceIndices) {
    auto &gfxCoreHelper = getGfxCoreHelper();
    return gfxCoreHelper.createTimestampPacketAllocator(rootDeviceIndices, getMemoryManager(), getPreferredTagPoolSize(), getType(), osContext->getDeviceBitfield());
}
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::postInitFlagsSetup() {
    useNewResourceImplicitFlush = checkPlatformSupportsNewResourceImplicitFlush();
    int32_t overrideNewResourceImplicitFlush = DebugManager.flags.PerformImplicitFlushForNewResource.get();
    if (overrideNewResourceImplicitFlush != -1) {
        useNewResourceImplicitFlush = overrideNewResourceImplicitFlush == 0 ? false : true;
    }
    useGpuIdleImplicitFlush = checkPlatformSupportsGpuIdleImplicitFlush();
    int32_t overrideGpuIdleImplicitFlush = DebugManager.flags.PerformImplicitFlushForIdleGpu.get();
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
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::initializeDeviceWithFirstSubmission() {
    return flushTagUpdate();
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleFrontEndStateTransition(const DispatchFlags &dispatchFlags) {
    if (streamProperties.frontEndState.disableOverdispatch.value != -1) {
        lastAdditionalKernelExecInfo = streamProperties.frontEndState.disableOverdispatch.value == 1 ? AdditionalKernelExecInfo::DisableOverdispatch : AdditionalKernelExecInfo::NotSet;
    }
    if (streamProperties.frontEndState.computeDispatchAllWalkerEnable.value != -1) {
        lastKernelExecutionType = streamProperties.frontEndState.computeDispatchAllWalkerEnable.value == 1 ? KernelExecutionType::Concurrent : KernelExecutionType::Default;
    }

    if (feSupportFlags.disableOverdispatch &&
        (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotApplicable && lastAdditionalKernelExecInfo != dispatchFlags.additionalKernelExecInfo)) {
        setMediaVFEStateDirty(true);
    }

    if (feSupportFlags.computeDispatchAllWalker &&
        (dispatchFlags.kernelExecutionType != KernelExecutionType::NotApplicable && lastKernelExecutionType != dispatchFlags.kernelExecutionType)) {
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
    if (dispatchFlags.l3CacheSettings != L3CachingSettings::NotApplicable) {
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
    if (dispatchFlags.memoryCompressionState != MemoryCompressionState::NotApplicable) {
        memoryCompressionState = dispatchFlags.memoryCompressionState;
    }
    if (memoryCompressionState != this->lastMemoryCompressionState) {
        isStateBaseAddressDirty = true;
        this->lastMemoryCompressionState = memoryCompressionState;
    }

    if (this->sbaSupportFlags.globalAtomics) {
        if (this->streamProperties.stateBaseAddress.globalAtomics.value != -1) {
            this->lastSentUseGlobalAtomics = !!this->streamProperties.stateBaseAddress.globalAtomics.value;
        }

        bool globalAtomics = (this->isMultiOsContextCapable() || dispatchFlags.areMultipleSubDevicesInContext) && dispatchFlags.useGlobalAtomics;
        if (this->lastSentUseGlobalAtomics != globalAtomics) {
            isStateBaseAddressDirty = true;
            this->lastSentUseGlobalAtomics = globalAtomics;
        }
        this->streamProperties.stateBaseAddress.setPropertyGlobalAtomics(globalAtomics, false);
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

    auto &hwInfo = this->peekHwInfo();
    const bool hasDsh = hwInfo.capabilityTable.supportsImages && dsh != nullptr;

    bool dshDirty = hasDsh ? dshState.updateAndCheck(dsh) : false;
    bool iohDirty = iohState.updateAndCheck(ioh);
    bool sshDirty = ssh != nullptr ? sshState.updateAndCheck(ssh) : false;

    bool bindingTablePoolCommandNeeded = sshDirty && (ssh->getGraphicsAllocation() != globalStatelessHeapAllocation);

    if (dshDirty) {
        int64_t dynamicStateBaseAddress = dsh->getHeapGpuBase();
        size_t dynamicStateSize = dsh->getHeapSizeInPages();
        this->streamProperties.stateBaseAddress.setPropertiesDynamicState(dynamicStateBaseAddress, dynamicStateSize);
    }
    if (iohDirty) {
        int64_t indirectObjectBaseAddress = ioh->getHeapGpuBase();
        size_t indirectObjectSize = ioh->getHeapSizeInPages();
        this->streamProperties.stateBaseAddress.setPropertiesIndirectState(indirectObjectBaseAddress, indirectObjectSize);
    }
    if (sshDirty) {
        int64_t surfaceStateBaseAddress = ssh->getHeapGpuBase();
        size_t surfaceStateSize = ssh->getHeapSizeInPages();
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
    if (is64bit && scratchSpaceController->getScratchSpaceAllocation() && !force32BitAllocations) {
        newGshBase = scratchSpaceController->calculateNewGSH();
    } else if (is64bit && force32BitAllocations && dispatchFlags.gsba32BitRequired) {
        bool useLocalMemory = scratchSpaceController->getScratchSpaceAllocation() ? scratchSpaceController->getScratchSpaceAllocation()->isAllocatedInLocalMemoryPool() : false;
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
    bool debuggingEnabled = device.getDebugger() != nullptr;

    EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(csrCommandStream, rootDeviceEnvironment, isRcs(), this->dcFlushSupport);
    EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(csrCommandStream, pipelineSelectArgs, true, rootDeviceEnvironment, isRcs());

    auto stateBaseAddressCmdOffset = csrCommandStream.getUsed();
    auto instructionHeapBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));
    auto bindlessSurfStateBase = 0ull;

    if (device.getBindlessHeapsHelper()) {
        bindlessSurfStateBase = device.getBindlessHeapsHelper()->getGlobalHeapsBase();
    }

    STATE_BASE_ADDRESS stateBaseAddressCmd;
    StateBaseAddressHelperArgs<GfxFamily> args = {
        generalStateBaseAddress,                  // generalStateBaseAddress
        indirectObjectStateBaseAddress,           // indirectObjectHeapBaseAddress
        instructionHeapBaseAddress,               // instructionHeapBaseAddress
        0,                                        // globalHeapsBaseAddress
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
        false,                                    // useGlobalHeapsBaseAddress
        isMultiOsContextCapable(),                // isMultiOsContextCapable
        this->lastSentUseGlobalAtomics,           // useGlobalAtomics
        areMultipleSubDevicesInContext,           // areMultipleSubDevicesInContext
        false,                                    // overrideSurfaceStateBaseAddress
        debuggingEnabled,                         // isDebuggerActive
        this->doubleSbaWa                         // doubleSbaWa
    };

    StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(args, csrCommandStream);

    bool sbaTrackingEnabled = debuggingEnabled;
    if (sbaTrackingEnabled) {
        device.getL0Debugger()->programSbaAddressLoad(csrCommandStream,
                                                      device.getL0Debugger()->getSbaTrackingBuffer(this->getOsContext().getContextId())->getGpuAddress());
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

    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        collectStateBaseAddresPatchInfo(commandStream.getGraphicsAllocation()->getGpuAddress(), stateBaseAddressCmdOffset, dsh, ioh, ssh, generalStateBaseAddress,
                                        device.getDeviceInfo().imageSupport);
    }
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
    } else {
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
    } else {
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
                                                   requiredScratchSize,
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
    } else {
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
    } else {
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

    if (this->getPreemptionMode() == PreemptionMode::Initial) {
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

        if (this->getPreemptionMode() == PreemptionMode::Initial) {
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

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    if (workPartitionAllocation) {
        makeResident(*workPartitionAllocation);
    }

    if (device.getRTMemoryBackedBuffer()) {
        makeResident(*device.getRTMemoryBackedBuffer());
    }

    if (flushData.estimatedSize > 0) {
        makeResident(*csrStream.getGraphicsAllocation());
    }

    if (preemptionAllocation) {
        makeResident(*preemptionAllocation);
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
    if (dispatchFlags.blockingAppend) {
        auto address = getTagAllocation()->getGpuAddress();

        PipeControlArgs args = {};
        args.dcFlushEnable = this->dcFlushSupport;
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        args.workloadPartitionOffset = isMultiTileOperationEnabled();
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            immediateCommandStream,
            PostSyncMode::ImmediateData,
            address,
            this->taskCount + 1,
            peekRootDeviceEnvironment(),
            args);
    }

    makeResident(*immediateCommandStream.getGraphicsAllocation());

    programEndingCmd(immediateCommandStream, &flushData.endPtr, isDirectSubmissionEnabled(), dispatchFlags.hasRelaxedOrderingDependencies);
    EncodeNoop<GfxFamily>::alignToCacheLine(immediateCommandStream);
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

    constexpr bool immediateLowPriority = false;
    constexpr QueueThrottle immediateThrottle = QueueThrottle::MEDIUM;
    constexpr uint64_t immediateSliceCount = QueueSliceCount::defaultSliceCount;

    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, chainedBatchBufferStartOffset, taskStartAddress, chainedBatchBuffer,
                            immediateLowPriority, immediateThrottle, immediateSliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, flushData.endPtr, this->getNumClients(), hasStallingCmds,
                            dispatchFlags.hasRelaxedOrderingDependencies, dispatchFlags.blockingAppend};
    updateStreamTaskCount(streamToSubmit, taskCount + 1);

    auto submissionStatus = flushHandler(batchBuffer, this->getResidencyAllocations());
    if (submissionStatus != SubmissionStatus::SUCCESS) {
        --this->latestSentTaskCount;
        updateStreamTaskCount(streamToSubmit, taskCount);

        CompletionStamp completionStamp = {CompletionStamp::getTaskCountFromSubmissionStatusError(submissionStatus)};
        return completionStamp;
    } else {
        if (dispatchFlags.blockingAppend) {
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

} // namespace NEO
