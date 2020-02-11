/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/linear_stream.h"
#include "core/command_stream/preemption.h"
#include "core/command_stream/scratch_space_controller_base.h"
#include "core/debug_settings/debug_settings_manager.h"
#include "core/device/device.h"
#include "core/execution_environment/root_device_environment.h"
#include "core/gmm_helper/page_table_mngr.h"
#include "core/helpers/blit_commands_helper.h"
#include "core/helpers/cache_policy.h"
#include "core/helpers/flush_stamp.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/preamble.h"
#include "core/helpers/ptr_math.h"
#include "core/helpers/state_base_address.h"
#include "core/helpers/timestamp_packet.h"
#include "core/indirect_heap/indirect_heap.h"
#include "core/memory_manager/internal_allocation_storage.h"
#include "core/memory_manager/memory_manager.h"
#include "core/os_interface/os_context.h"
#include "core/utilities/tag_allocator.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/gtpin/gtpin_notify.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/helpers/hardware_commands_helper.h"

#include "command_stream_receiver_hw_ext.inl"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
    : CommandStreamReceiver(executionEnvironment, rootDeviceIndex) {

    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    localMemoryEnabled = hwHelper.getEnableLocalMemory(peekHwInfo());

    requiredThreadArbitrationPolicy = PreambleHelper<GfxFamily>::getDefaultThreadArbitrationPolicy();
    resetKmdNotifyHelper(new KmdNotifyHelper(&peekHwInfo().capabilityTable.kmdNotifyProperties));
    flatBatchBufferHelper.reset(new FlatBatchBufferHelperHw<GfxFamily>(executionEnvironment));
    defaultSshSize = getSshHeapSize();

    timestampPacketWriteEnabled = hwHelper.timestampPacketWriteSupported();
    if (DebugManager.flags.EnableTimestampPacket.get() != -1) {
        timestampPacketWriteEnabled = !!DebugManager.flags.EnableTimestampPacket.get();
    }
    createScratchSpaceController();
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    return true;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addBatchBufferEnd(LinearStream &commandStream, void **patchLocation) {
    typedef typename GfxFamily::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;

    auto pCmd = (MI_BATCH_BUFFER_END *)commandStream.getSpace(sizeof(MI_BATCH_BUFFER_END));
    *pCmd = GfxFamily::cmdInitBatchBufferEnd;
    if (patchLocation) {
        *patchLocation = pCmd;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary) {
    *commandBufferMemory = GfxFamily::cmdInitBatchBufferStart;
    commandBufferMemory->setBatchBufferStartAddressGraphicsaddress472(startAddress);
    commandBufferMemory->setAddressSpaceIndicator(MI_BATCH_BUFFER_START::ADDRESS_SPACE_INDICATOR_PPGTT);
    if (secondary) {
        commandBufferMemory->setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_SECOND_LEVEL_BATCH);
    }
    if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
        flatBatchBufferHelper->registerBatchBufferStartAddress(reinterpret_cast<uint64_t>(commandBufferMemory), startAddress);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::alignToCacheLine(LinearStream &commandStream) {
    auto used = commandStream.getUsed();
    auto alignment = MemoryConstants::cacheLineSize;
    auto partialCacheline = used & (alignment - 1);
    if (partialCacheline) {
        auto amountToPad = alignment - partialCacheline;
        auto pCmd = commandStream.getSpace(amountToPad);
        memset(pCmd, 0, amountToPad);
    }
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
    if (!this->isPreambleSent || this->lastSentThreadArbitrationPolicy != this->requiredThreadArbitrationPolicy) {
        size += PreambleHelper<GfxFamily>::getThreadArbitrationCommandsSize();
    }

    if (DebugManager.flags.ForcePerDssBackedBufferProgramming.get()) {
        if (!this->isPreambleSent) {
            size += PreambleHelper<GfxFamily>::getPerDssBackedBufferCommandsSize(device.getHardwareInfo());
        }
    }
    return size;
}

template <typename GfxFamily>
inline typename GfxFamily::PIPE_CONTROL *CommandStreamReceiverHw<GfxFamily>::addPipeControlCmd(LinearStream &commandStream) {
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    auto pCmd = reinterpret_cast<PIPE_CONTROL *>(commandStream.getSpace(sizeof(PIPE_CONTROL)));
    *pCmd = GfxFamily::cmdInitPipeControl;
    pCmd->setCommandStreamerStallEnable(true);
    return pCmd;
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTask(
    LinearStream &commandStreamTask,
    size_t commandStreamStartTask,
    const IndirectHeap &dsh,
    const IndirectHeap &ioh,
    const IndirectHeap &ssh,
    uint32_t taskLevel,
    DispatchFlags &dispatchFlags,
    Device &device) {
    typedef typename GfxFamily::MI_BATCH_BUFFER_START MI_BATCH_BUFFER_START;
    typedef typename GfxFamily::MI_BATCH_BUFFER_END MI_BATCH_BUFFER_END;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    typedef typename GfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    DEBUG_BREAK_IF(&commandStreamTask == &commandStream);
    DEBUG_BREAK_IF(!(dispatchFlags.preemptionMode == PreemptionMode::Disabled ? device.getPreemptionMode() == PreemptionMode::Disabled : true));
    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::levelNotReady);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskLevel", taskLevel);

    auto levelClosed = false;
    void *currentPipeControlForNooping = nullptr;
    void *epiloguePipeControlLocation = nullptr;

    if (DebugManager.flags.ForceCsrFlushing.get()) {
        flushBatchedSubmissions();
    }
    if (detectInitProgrammingFlagsRequired(dispatchFlags)) {
        initProgrammingFlags();
    }

    if (dispatchFlags.blocking || dispatchFlags.dcFlush || dispatchFlags.guardCommandBufferWithPipeControl) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            //for ImmediateDispatch we will send this right away, therefore this pipe control will close the level
            //for BatchedSubmissions it will be nooped and only last ppc in batch will be emitted.
            levelClosed = true;
            //if we guard with ppc, flush dc as well to speed up completion latency
            if (dispatchFlags.guardCommandBufferWithPipeControl) {
                dispatchFlags.dcFlush = true;
            }
        }

        epiloguePipeControlLocation = ptrOffset(commandStreamTask.getCpuBase(), commandStreamTask.getUsed());

        if ((dispatchFlags.outOfOrderExecutionAllowed || timestampPacketWriteEnabled) &&
            !dispatchFlags.dcFlush) {
            currentPipeControlForNooping = epiloguePipeControlLocation;
        }

        auto address = getTagAllocation()->getGpuAddress();
        PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(commandStreamTask,
                                                                                   PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                                                                                   address, taskCount + 1, dispatchFlags.dcFlush, peekHwInfo());

        this->latestSentTaskCount = taskCount + 1;
        DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", taskCount);
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

    if (DebugManager.flags.ForceSLML3Config.get()) {
        dispatchFlags.useSLM = true;
    }
    if (DebugManager.flags.OverrideThreadArbitrationPolicy.get() != -1) {
        dispatchFlags.threadArbitrationPolicy = static_cast<uint32_t>(DebugManager.flags.OverrideThreadArbitrationPolicy.get());
    }

    auto newL3Config = PreambleHelper<GfxFamily>::getL3Config(peekHwInfo(), dispatchFlags.useSLM);

    csrSizeRequestFlags.l3ConfigChanged = this->lastSentL3Config != newL3Config;
    csrSizeRequestFlags.coherencyRequestChanged = this->lastSentCoherencyRequest != static_cast<int8_t>(dispatchFlags.requiresCoherency);
    csrSizeRequestFlags.preemptionRequestChanged = this->lastPreemptionMode != dispatchFlags.preemptionMode;
    csrSizeRequestFlags.mediaSamplerConfigChanged = this->lastMediaSamplerConfig != static_cast<int8_t>(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    csrSizeRequestFlags.specialPipelineSelectModeChanged = this->lastSpecialPipelineSelectMode != dispatchFlags.pipelineSelectArgs.specialPipelineSelectMode;
    csrSizeRequestFlags.numGrfRequiredChanged = this->lastSentNumGrfRequired != dispatchFlags.numGrfRequired;
    lastSentNumGrfRequired = dispatchFlags.numGrfRequired;

    if (dispatchFlags.threadArbitrationPolicy != ThreadArbitrationPolicy::NotPresent) {
        this->requiredThreadArbitrationPolicy = dispatchFlags.threadArbitrationPolicy;
    }

    auto force32BitAllocations = getMemoryManager()->peekForce32BitAllocations();
    bool stateBaseAddressDirty = false;

    bool checkVfeStateDirty = false;
    if (requiredScratchSize || requiredPrivateScratchSize) {
        scratchSpaceController->setRequiredScratchSpace(ssh.getCpuBase(),
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

    auto &commandStreamCSR = this->getCS(getRequiredCmdStreamSizeAligned(dispatchFlags, device));
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    TimestampPacketHelper::programCsrDependencies<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);

    if (stallingPipeControlOnNextFlushRequired) {
        programStallingPipeControlForBarrier(commandStreamCSR, dispatchFlags);
    }

    programEngineModeCommands(commandStreamCSR, dispatchFlags);
    if (executionEnvironment.rootDeviceEnvironments[device.getRootDeviceIndex()]->pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = executionEnvironment.rootDeviceEnvironments[device.getRootDeviceIndex()]->pageTableManager->initPageTableManagerRegisters(this);
    }
    programEnginePrologue(commandStreamCSR);
    programComputeMode(commandStreamCSR, dispatchFlags);
    programL3(commandStreamCSR, dispatchFlags, newL3Config);
    programPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs);
    programPreamble(commandStreamCSR, device, dispatchFlags, newL3Config);
    programMediaSampler(commandStreamCSR, dispatchFlags);

    if (this->lastSentThreadArbitrationPolicy != this->requiredThreadArbitrationPolicy) {
        PreambleHelper<GfxFamily>::programThreadArbitration(&commandStreamCSR, this->requiredThreadArbitrationPolicy);
        this->lastSentThreadArbitrationPolicy = this->requiredThreadArbitrationPolicy;
    }

    stateBaseAddressDirty |= ((GSBAFor32BitProgrammed ^ dispatchFlags.gsba32BitRequired) && force32BitAllocations);

    programVFEState(commandStreamCSR, dispatchFlags, device.getDeviceInfo().maxFrontEndThreads);

    programPreemption(commandStreamCSR, dispatchFlags);

    bool dshDirty = dshState.updateAndCheck(&dsh);
    bool iohDirty = iohState.updateAndCheck(&ioh);
    bool sshDirty = sshState.updateAndCheck(&ssh);

    auto isStateBaseAddressDirty = dshDirty || iohDirty || sshDirty || stateBaseAddressDirty;

    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    auto l3On = dispatchFlags.l3CacheSettings != L3CachingSettings::l3CacheOff;
    auto l1On = dispatchFlags.l3CacheSettings == L3CachingSettings::l3AndL1On;
    auto mocsIndex = hwHelper.getMocsIndex(*device.getGmmHelper(), l3On, l1On);

    if (mocsIndex != latestSentStatelessMocsConfig) {
        isStateBaseAddressDirty = true;
        latestSentStatelessMocsConfig = mocsIndex;
    }

    //Reprogram state base address if required
    if (isStateBaseAddressDirty || device.isDebuggerActive()) {
        addPipeControlBeforeStateBaseAddress(commandStreamCSR);

        uint64_t newGSHbase = 0;
        GSBAFor32BitProgrammed = false;
        if (is64bit && scratchSpaceController->getScratchSpaceAllocation() && !force32BitAllocations) {
            newGSHbase = scratchSpaceController->calculateNewGSH();
        } else if (is64bit && force32BitAllocations && dispatchFlags.gsba32BitRequired) {
            newGSHbase = getMemoryManager()->getExternalHeapBaseAddress(rootDeviceIndex);
            GSBAFor32BitProgrammed = true;
        }

        auto stateBaseAddressCmdOffset = commandStreamCSR.getUsed();

        StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
            commandStreamCSR,
            &dsh,
            &ioh,
            &ssh,
            newGSHbase,
            true,
            mocsIndex,
            getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex),
            true,
            device.getGmmHelper(),
            isMultiOsContextCapable());

        if (sshDirty) {
            bindingTableBaseAddressRequired = true;
        }

        if (bindingTableBaseAddressRequired) {
            StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(commandStreamCSR, ssh, device.getGmmHelper());
            bindingTableBaseAddressRequired = false;
        }

        programStateSip(commandStreamCSR, device);

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            collectStateBaseAddresPatchInfo(commandStream.getGraphicsAllocation()->getGpuAddress(), stateBaseAddressCmdOffset, dsh, ioh, ssh, newGSHbase);
        }
    }

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskLevel", (uint32_t)this->taskLevel);

    if (executionEnvironment.getHardwareInfo()->workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
        if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
            auto pCmd = addPipeControlCmd(commandStreamCSR);
            pCmd->setTextureCacheInvalidationEnable(true);
            if (this->samplerCacheFlushRequired == SamplerCacheFlushState::samplerCacheFlushBefore) {
                this->samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushAfter;
            } else {
                this->samplerCacheFlushRequired = SamplerCacheFlushState::samplerCacheFlushNotRequired;
            }
        }
    }

    if (experimentalCmdBuffer.get() != nullptr) {
        size_t startingOffset = experimentalCmdBuffer->programExperimentalCommandBuffer<GfxFamily>();
        experimentalCmdBuffer->injectBufferStart<GfxFamily>(commandStreamCSR, startingOffset);
    }

    if (requiresInstructionCacheFlush) {
        auto pipeControl = PipeControlHelper<GfxFamily>::addPipeControl(commandStreamCSR, false);
        pipeControl->setInstructionCacheInvalidateEnable(true);
        requiresInstructionCacheFlush = false;
    }

    // Add a PC if we have a dependency on a previous walker to avoid concurrency issues.
    if (taskLevel > this->taskLevel) {
        if (!timestampPacketWriteEnabled) {
            PipeControlHelper<GfxFamily>::addPipeControl(commandStreamCSR, false);
        }
        this->taskLevel = taskLevel;
        DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskCount", this->taskCount);
    }

    auto dshAllocation = dsh.getGraphicsAllocation();
    auto iohAllocation = ioh.getGraphicsAllocation();
    auto sshAllocation = ssh.getGraphicsAllocation();

    this->makeResident(*dshAllocation);
    dshAllocation->setEvictable(false);
    this->makeResident(*iohAllocation);
    this->makeResident(*sshAllocation);
    iohAllocation->setEvictable(false);

    this->makeResident(*tagAllocation);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    if (preemptionAllocation) {
        makeResident(*preemptionAllocation);
    }

    if (dispatchFlags.preemptionMode == PreemptionMode::MidThread || device.isDebuggerActive()) {
        makeResident(*SipKernel::getSipKernelAllocation(device));
        if (debugSurface) {
            makeResident(*debugSurface);
        }
    }

    if (experimentalCmdBuffer.get() != nullptr) {
        experimentalCmdBuffer->makeResidentAllocations();
    }

    // If the CSR has work in its CS, flush it before the task
    bool submitTask = commandStreamStartTask != commandStreamTask.getUsed();
    bool submitCSR = (commandStreamStartCSR != commandStreamCSR.getUsed()) || this->isMultiOsContextCapable();
    bool submitCommandStreamFromCsr = false;
    void *bbEndLocation = nullptr;
    auto bbEndPaddingSize = this->dispatchMode == DispatchMode::ImmediateDispatch ? 0 : sizeof(MI_BATCH_BUFFER_START) - sizeof(MI_BATCH_BUFFER_END);
    size_t chainedBatchBufferStartOffset = 0;
    GraphicsAllocation *chainedBatchBuffer = nullptr;

    if (submitTask) {
        this->addBatchBufferEnd(commandStreamTask, &bbEndLocation);
        this->emitNoop(commandStreamTask, bbEndPaddingSize);
        this->alignToCacheLine(commandStreamTask);

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
            this->alignToCacheLine(commandStreamCSR);
            submitCommandStreamFromCsr = true;
        } else if (dispatchFlags.epilogueRequired) {
            this->makeResident(*commandStreamCSR.getGraphicsAllocation());
        }
        this->programEpilogue(commandStreamCSR, &bbEndLocation, dispatchFlags);

    } else if (submitCSR) {
        this->addBatchBufferEnd(commandStreamCSR, &bbEndLocation);
        this->emitNoop(commandStreamCSR, bbEndPaddingSize);
        this->alignToCacheLine(commandStreamCSR);
        DEBUG_BREAK_IF(commandStreamCSR.getUsed() > commandStreamCSR.getMaxAvailableSpace());
        submitCommandStreamFromCsr = true;
    }

    size_t startOffset = submitCommandStreamFromCsr ? commandStreamStartCSR : commandStreamStartTask;
    auto &streamToSubmit = submitCommandStreamFromCsr ? commandStreamCSR : commandStreamTask;
    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, chainedBatchBufferStartOffset, chainedBatchBuffer, dispatchFlags.requiresCoherency, dispatchFlags.lowPriority, dispatchFlags.throttle, dispatchFlags.sliceCount, streamToSubmit.getUsed(), &streamToSubmit};

    if (submitCSR | submitTask) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            this->flush(batchBuffer, this->getResidencyAllocations());
            this->latestFlushedTaskCount = this->taskCount + 1;
            this->makeSurfacePackNonResident(this->getResidencyAllocations());
        } else {
            auto commandBuffer = new CommandBuffer(device);
            commandBuffer->batchBuffer = batchBuffer;
            commandBuffer->surfaces.swap(this->getResidencyAllocations());
            commandBuffer->batchBufferEndLocation = bbEndLocation;
            commandBuffer->taskCount = this->taskCount + 1;
            commandBuffer->flushStamp->replaceStampObject(dispatchFlags.flushStampReference);
            commandBuffer->pipeControlThatMayBeErasedLocation = currentPipeControlForNooping;
            commandBuffer->epiloguePipeControlLocation = epiloguePipeControlLocation;
            this->submissionAggregator->recordCommandBuffer(commandBuffer);
        }
    } else {
        this->makeSurfacePackNonResident(this->getResidencyAllocations());
    }

    //check if we are not over the budget, if we are do implicit flush
    if (getMemoryManager()->isMemoryBudgetExhausted()) {
        if (this->totalMemoryUsed >= device.getDeviceInfo().globalMemSize / 4) {
            dispatchFlags.implicitFlush = true;
        }
    }

    if (this->dispatchMode == DispatchMode::BatchedDispatch && (dispatchFlags.blocking || dispatchFlags.implicitFlush)) {
        this->flushBatchedSubmissions();
    }

    ++taskCount;
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", taskCount);
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", tagAddress ? *tagAddress : 0);

    CompletionStamp completionStamp = {
        taskCount,
        this->taskLevel,
        flushStamp->peekStamp()};

    this->taskLevel += levelClosed ? 1 : 0;

    if (gtpinIsGTPinInitialized()) {
        gtpinNotifyFlushTask(completionStamp.taskCount);
    }

    return completionStamp;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPipeControlForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags) {
    stallingPipeControlOnNextFlushRequired = false;

    PIPE_CONTROL *stallingPipeControlCmd;
    auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;

    if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() != 0) {
        auto barrierTimestampPacketGpuAddress = dispatchFlags.barrierTimestampPacketNodes->peekNodes()[0]->getGpuAddress() +
                                                offsetof(TimestampPacketStorage, packets[0].contextEnd);

        stallingPipeControlCmd = PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
            cmdStream, PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
            barrierTimestampPacketGpuAddress, 0, false, peekHwInfo());

        dispatchFlags.barrierTimestampPacketNodes->makeResident(*this);
    } else {
        stallingPipeControlCmd = PipeControlHelper<GfxFamily>::addPipeControl(cmdStream, false);
    }

    stallingPipeControlCmd->setCommandStreamerStallEnable(true);
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
        auto pipeControlLocationSize = PipeControlHelper<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo());
        void *currentPipeControlForNooping = nullptr;
        void *epiloguePipeControlLocation = nullptr;

        while (!commandBufferList.peekIsEmpty()) {
            size_t totalUsedSize = 0u;
            this->submissionAggregator->aggregateCommandBuffers(resourcePackage, totalUsedSize, totalMemoryBudget, osContext->getContextId());
            auto primaryCmdBuffer = commandBufferList.removeFrontOne();
            auto nextCommandBuffer = commandBufferList.peekHead();
            auto currentBBendLocation = primaryCmdBuffer->batchBufferEndLocation;
            auto lastTaskCount = primaryCmdBuffer->taskCount;

            FlushStampUpdateHelper flushStampUpdateHelper;
            flushStampUpdateHelper.insert(primaryCmdBuffer->flushStamp->getStampReference());

            currentPipeControlForNooping = primaryCmdBuffer->pipeControlThatMayBeErasedLocation;
            epiloguePipeControlLocation = primaryCmdBuffer->epiloguePipeControlLocation;

            if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                flatBatchBufferHelper->registerCommandChunk(primaryCmdBuffer.get()->batchBuffer, sizeof(MI_BATCH_BUFFER_START));
            }
            while (nextCommandBuffer && nextCommandBuffer->inspectionId == primaryCmdBuffer->inspectionId) {
                //noop pipe control
                if (currentPipeControlForNooping) {
                    if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
                        flatBatchBufferHelper->removePipeControlData(pipeControlLocationSize, currentPipeControlForNooping, peekHwInfo());
                    }
                    memset(currentPipeControlForNooping, 0, pipeControlLocationSize);
                }
                //obtain next candidate for nooping
                currentPipeControlForNooping = nextCommandBuffer->pipeControlThatMayBeErasedLocation;
                //track epilogue pipe control
                epiloguePipeControlLocation = nextCommandBuffer->epiloguePipeControlLocation;

                flushStampUpdateHelper.insert(nextCommandBuffer->flushStamp->getStampReference());
                auto nextCommandBufferAddress = nextCommandBuffer->batchBuffer.commandBufferAllocation->getGpuAddress();
                auto offsetedCommandBuffer = (uint64_t)ptrOffset(nextCommandBufferAddress, nextCommandBuffer->batchBuffer.startOffset);
                addBatchBufferStart((MI_BATCH_BUFFER_START *)currentBBendLocation, offsetedCommandBuffer, false);
                if (DebugManager.flags.FlattenBatchBufferForAUBDump.get()) {
                    flatBatchBufferHelper->registerCommandChunk(nextCommandBuffer->batchBuffer, sizeof(MI_BATCH_BUFFER_START));
                }

                currentBBendLocation = nextCommandBuffer->batchBufferEndLocation;
                lastTaskCount = nextCommandBuffer->taskCount;
                nextCommandBuffer = nextCommandBuffer->next;
                commandBufferList.removeFrontOne();
            }
            surfacesForSubmit.reserve(resourcePackage.size() + 1);
            for (auto &surface : resourcePackage) {
                surfacesForSubmit.push_back(surface);
            }

            //make sure we flush DC if needed
            if (epiloguePipeControlLocation) {
                bool flushDcInEpilogue = true;
                if (DebugManager.flags.DisableDcFlushInEpilogue.get()) {
                    flushDcInEpilogue = false;
                }
                ((PIPE_CONTROL *)epiloguePipeControlLocation)->setDcFlushEnable(flushDcInEpilogue);
            }

            if (!this->flush(primaryCmdBuffer->batchBuffer, surfacesForSubmit)) {
                submitResult = false;
                break;
            }

            //after flush task level is closed
            this->taskLevel++;

            flushStampUpdateHelper.updateAll(flushStamp->peekStamp());

            this->latestFlushedTaskCount = lastTaskCount;
            this->makeSurfacePackNonResident(surfacesForSubmit);
            resourcePackage.clear();
        }
        this->totalMemoryUsed = 0;
    }

    return submitResult;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSizeAligned(const DispatchFlags &dispatchFlags, Device &device) {
    size_t size = getRequiredCmdStreamSize(dispatchFlags, device);
    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamSize(const DispatchFlags &dispatchFlags, Device &device) {
    size_t size = getRequiredCmdSizeForPreamble(device);
    size += getRequiredStateBaseAddressSize();
    if (!this->isStateSipSent || device.isDebuggerActive()) {
        size += PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(device);
    }
    size += PipeControlHelper<GfxFamily>::getSizeForSinglePipeControl();
    size += sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);

    size += getCmdSizeForL3Config();
    size += getCmdSizeForComputeMode();
    size += getCmdSizeForMediaSampler(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    size += getCmdSizeForPipelineSelect();
    size += getCmdSizeForPreemption(dispatchFlags);
    size += getCmdSizeForEpilogue(dispatchFlags);
    size += getCmdSizeForPrologue(dispatchFlags);

    if (executionEnvironment.getHardwareInfo()->workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
        if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }
    if (experimentalCmdBuffer.get() != nullptr) {
        size += experimentalCmdBuffer->getRequiredInjectionSize<GfxFamily>();
    }

    size += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.csrDependencies);

    if (stallingPipeControlOnNextFlushRequired) {
        auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;
        if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() > 0) {
            size += PipeControlHelper<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo());
        } else {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }

    if (requiresInstructionCacheFlush) {
        size += sizeof(typename GfxFamily::PIPE_CONTROL);
    }

    return size;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPipelineSelect() const {

    size_t size = 0;
    if (csrSizeRequestFlags.mediaSamplerConfigChanged ||
        csrSizeRequestFlags.specialPipelineSelectModeChanged ||
        !isPreambleSent) {
        size += PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(peekHwInfo());
    }
    return size;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::emitNoop(LinearStream &commandStream, size_t bytesToUpdate) {
    if (bytesToUpdate) {
        auto ptr = commandStream.getSpace(bytesToUpdate);
        memset(ptr, 0, bytesToUpdate);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, bool forcePowerSavingMode) {
    int64_t waitTimeout = 0;
    bool enableTimeout = kmdNotifyHelper->obtainTimeoutParams(waitTimeout, useQuickKmdSleep, *getTagAddress(), taskCountToWait, flushStampToWait, forcePowerSavingMode);

    auto status = waitForCompletionWithTimeout(enableTimeout, waitTimeout, taskCountToWait);
    if (!status) {
        waitForFlushStamp(flushStampToWait);
        //now call blocking wait, this is to ensure that task count is reached
        waitForCompletionWithTimeout(false, 0, taskCountToWait);
    }
    UNRECOVERABLE_IF(*getTagAddress() < taskCountToWait);

    if (kmdNotifyHelper->quickKmdSleepForSporadicWaitsEnabled()) {
        kmdNotifyHelper->updateLastWaitForCompletionTimestamp();
    }
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
    if (!this->isStateSipSent || device.isDebuggerActive()) {
        PreemptionHelper::programStateSip<GfxFamily>(cmdStream, device);
        this->isStateSipSent = true;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programPreamble(LinearStream &csr, Device &device, DispatchFlags &dispatchFlags, uint32_t &newL3Config) {
    if (!this->isPreambleSent) {
        GraphicsAllocation *perDssBackedBufferToUse = dispatchFlags.usePerDssBackedBuffer ? this->perDssBackedBuffer : nullptr;
        PreambleHelper<GfxFamily>::programPreamble(&csr, device, newL3Config, this->requiredThreadArbitrationPolicy, this->preemptionAllocation, perDssBackedBufferToUse);
        this->isPreambleSent = true;
        this->lastSentL3Config = newL3Config;
        this->lastSentThreadArbitrationPolicy = this->requiredThreadArbitrationPolicy;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programVFEState(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t maxFrontEndThreads) {
    if (mediaVfeStateDirty) {
        auto commandOffset = PreambleHelper<GfxFamily>::programVFEState(&csr, peekHwInfo(), requiredScratchSize, getScratchPatchAddress(), maxFrontEndThreads, getOsContext().getEngineType());
        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            flatBatchBufferHelper->collectScratchSpacePatchInfo(getScratchPatchAddress(), commandOffset, csr);
        }
        setMediaVFEStateDirty(false);
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
    const LinearStream &dsh,
    const LinearStream &ioh,
    const LinearStream &ssh,
    uint64_t generalStateBase) {

    typedef typename GfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;

    PatchInfoData dynamicStatePatchInfo = {dsh.getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::DynamicStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::DYNAMICSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
    PatchInfoData generalStatePatchInfo = {generalStateBase, 0u, PatchInfoAllocationType::GeneralStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::GENERALSTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
    PatchInfoData surfaceStatePatchInfo = {ssh.getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::SurfaceStateHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::SURFACESTATEBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
    PatchInfoData indirectObjectPatchInfo = {ioh.getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::IndirectObjectHeap, baseAddress, commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};

    flatBatchBufferHelper->setPatchInfoData(dynamicStatePatchInfo);
    flatBatchBufferHelper->setPatchInfoData(generalStatePatchInfo);
    flatBatchBufferHelper->setPatchInfoData(surfaceStatePatchInfo);
    flatBatchBufferHelper->setPatchInfoData(indirectObjectPatchInfo);
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
void CommandStreamReceiverHw<GfxFamily>::addClearSLMWorkAround(typename GfxFamily::PIPE_CONTROL *pCmd) {
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
uint32_t CommandStreamReceiverHw<GfxFamily>::blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    auto lock = obtainUniqueOwnership();

    auto &commandStream = getCS(BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitPropertiesContainer, peekHwInfo()));
    auto commandStreamStart = commandStream.getUsed();
    auto newTaskCount = taskCount + 1;
    latestSentTaskCount = newTaskCount;

    programEnginePrologue(commandStream);

    for (auto &blitProperties : blitPropertiesContainer) {
        TimestampPacketHelper::programCsrDependencies<GfxFamily>(commandStream, blitProperties.csrDependencies);

        BlitCommandsHelper<GfxFamily>::dispatchBlitCommandsForBuffer(blitProperties, commandStream);

        if (blitProperties.outputTimestampPacket) {
            auto timestampPacketGpuAddress = blitProperties.outputTimestampPacket->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
            HardwareCommandsHelper<GfxFamily>::programMiFlushDw(commandStream, timestampPacketGpuAddress, 0);
            makeResident(*blitProperties.outputTimestampPacket->getBaseGraphicsAllocation());
        }

        blitProperties.csrDependencies.makeResident(*this);

        makeResident(*blitProperties.srcAllocation);
        makeResident(*blitProperties.dstAllocation);
    }

    PipeControlHelper<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), peekHwInfo());

    HardwareCommandsHelper<GfxFamily>::programMiFlushDw(commandStream, tagAllocation->getGpuAddress(), newTaskCount);

    PipeControlHelper<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), peekHwInfo());

    auto batchBufferEnd = reinterpret_cast<MI_BATCH_BUFFER_END *>(commandStream.getSpace(sizeof(MI_BATCH_BUFFER_END)));
    *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;

    alignToCacheLine(commandStream);

    makeResident(*tagAllocation);
    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    BatchBuffer batchBuffer{commandStream.getGraphicsAllocation(), commandStreamStart, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStream.getUsed(), &commandStream};

    flush(batchBuffer, getResidencyAllocations());
    makeSurfacePackNonResident(getResidencyAllocations());

    latestFlushedTaskCount = newTaskCount;
    taskCount = newTaskCount;
    auto flushStampToWait = flushStamp->peekStamp();

    lock.unlock();
    if (blocking) {
        waitForTaskCountWithKmdNotifyFallback(newTaskCount, flushStampToWait, false, false);
        internalAllocationStorage->cleanAllocationList(newTaskCount, TEMPORARY_ALLOCATION);
    }

    return newTaskCount;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEpilogue(LinearStream &csr, void **batchBufferEndLocation, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.epilogueRequired) {
        auto currentOffset = ptrDiff(csr.getSpace(0u), csr.getCpuBase());
        auto gpuAddress = ptrOffset(csr.getGraphicsAllocation()->getGpuAddress(), currentOffset);

        addBatchBufferStart(reinterpret_cast<typename GfxFamily::MI_BATCH_BUFFER_START *>(*batchBufferEndLocation), gpuAddress, false);
        this->programEpliogueCommands(csr, dispatchFlags);
        this->addBatchBufferEnd(csr, batchBufferEndLocation);
        this->alignToCacheLine(csr);
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForEpilogue(const DispatchFlags &dispatchFlags) const {
    if (dispatchFlags.epilogueRequired) {
        auto size = getCmdSizeForEpilogueCommands(dispatchFlags) + sizeof(typename GfxFamily::MI_BATCH_BUFFER_END);
        return alignUp(size, MemoryConstants::cacheLineSize);
    }
    return 0u;
}
template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEnginePrologue(LinearStream &csr) {
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPrologue(const DispatchFlags &dispatchFlags) const {
    return 0u;
}

} // namespace NEO
