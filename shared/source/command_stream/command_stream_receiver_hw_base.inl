/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/tag_allocator.h"

#include "command_stream_receiver_hw_ext.inl"

namespace NEO {

template <typename GfxFamily>
CommandStreamReceiverHw<GfxFamily>::~CommandStreamReceiverHw() = default;

template <typename GfxFamily>
CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(ExecutionEnvironment &executionEnvironment,
                                                            uint32_t rootDeviceIndex,
                                                            const DeviceBitfield deviceBitfield)
    : CommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {

    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    localMemoryEnabled = hwHelper.getEnableLocalMemory(peekHwInfo());

    requiredThreadArbitrationPolicy = hwHelper.getDefaultThreadArbitrationPolicy();
    resetKmdNotifyHelper(new KmdNotifyHelper(&peekHwInfo().capabilityTable.kmdNotifyProperties));
    flatBatchBufferHelper.reset(new FlatBatchBufferHelperHw<GfxFamily>(executionEnvironment));
    defaultSshSize = getSshHeapSize();
    canUse4GbHeaps = are4GbHeapsAvailable();

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
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    auto pCmd = commandStream.getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *pCmd = GfxFamily::cmdInitBatchBufferEnd;
    if (patchLocation) {
        *patchLocation = pCmd;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEndingCmd(LinearStream &commandStream, Device &device, void **patchLocation, bool directSubmissionEnabled) {
    if (directSubmissionEnabled) {
        *patchLocation = commandStream.getSpace(sizeof(MI_BATCH_BUFFER_START));
        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*patchLocation);
        MI_BATCH_BUFFER_START cmd = {};
        addBatchBufferStart(&cmd, 0ull, false);
        *bbStart = cmd;
    } else {
        PreemptionHelper::programStateSipEndWa<GfxFamily>(commandStream, device);
        this->addBatchBufferEnd(commandStream, patchLocation);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addBatchBufferStart(MI_BATCH_BUFFER_START *commandBufferMemory, uint64_t startAddress, bool secondary) {
    MI_BATCH_BUFFER_START cmd = GfxFamily::cmdInitBatchBufferStart;

    cmd.setBatchBufferStartAddressGraphicsaddress472(startAddress);
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
    if (!this->isPreambleSent) {
        if (DebugManager.flags.ForceSemaphoreDelayBetweenWaits.get() > -1) {
            size += PreambleHelper<GfxFamily>::getSemaphoreDelayCommandSize();
        }
    }
    return size;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlCmd(
    LinearStream &commandStream,
    PipeControlArgs &args) {
    MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);
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
    DEBUG_BREAK_IF(taskLevel >= CompletionStamp::notReady);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskLevel", taskLevel);

    auto levelClosed = false;
    bool implicitFlush = dispatchFlags.implicitFlush || dispatchFlags.blocking || DebugManager.flags.ForceImplicitFlush.get();
    void *currentPipeControlForNooping = nullptr;
    void *epiloguePipeControlLocation = nullptr;

    bool csrFlush = this->wasSubmittedToSingleSubdevice != dispatchFlags.useSingleSubdevice;

    csrFlush |= DebugManager.flags.ForceCsrFlushing.get();

    if (csrFlush) {
        flushBatchedSubmissions();
    }

    if (detectInitProgrammingFlagsRequired(dispatchFlags)) {
        initProgrammingFlags();
    }

    bool updateTag = false;
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

        updateTag = !isUpdateTagFromWaitEnabled();
        updateTag |= dispatchFlags.blocking;

        if (updateTag) {
            PipeControlArgs args(dispatchFlags.dcFlush);
            args.notifyEnable = isUsedNotifyEnableForPostSync();
            args.tlbInvalidation |= dispatchFlags.memoryMigrationRequired;
            MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
                commandStreamTask,
                PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                address,
                taskCount + 1,
                peekHwInfo(),
                args);
        } else {
            currentPipeControlForNooping = nullptr;
        }

        this->latestSentTaskCount = taskCount + 1;
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

    if (dispatchFlags.numGrfRequired == GrfConfig::NotApplicable) {
        dispatchFlags.numGrfRequired = lastSentNumGrfRequired;
    }

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

    if (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotApplicable && lastAdditionalKernelExecInfo != dispatchFlags.additionalKernelExecInfo) {
        setMediaVFEStateDirty(true);
    }

    if (dispatchFlags.kernelExecutionType != KernelExecutionType::NotApplicable && lastKernelExecutionType != dispatchFlags.kernelExecutionType) {
        setMediaVFEStateDirty(true);
    }

    auto &commandStreamCSR = this->getCS(getRequiredCmdStreamSizeAligned(dispatchFlags, device));
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);
    TimestampPacketHelper::programCsrDependenciesForForTaskCountContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);

    if (stallingPipeControlOnNextFlushRequired) {
        programStallingPipeControlForBarrier(commandStreamCSR, dispatchFlags);
    }

    programEngineModeCommands(commandStreamCSR, dispatchFlags);
    if (executionEnvironment.rootDeviceEnvironments[device.getRootDeviceIndex()]->pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = executionEnvironment.rootDeviceEnvironments[device.getRootDeviceIndex()]->pageTableManager->initPageTableManagerRegisters(this);
    }

    programHardwareContext(commandStreamCSR);
    programComputeMode(commandStreamCSR, dispatchFlags, device.getHardwareInfo());
    programPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs);
    programL3(commandStreamCSR, dispatchFlags, newL3Config);
    programPreamble(commandStreamCSR, device, dispatchFlags, newL3Config);
    programMediaSampler(commandStreamCSR, dispatchFlags);
    programPerDssBackedBuffer(commandStreamCSR, device, dispatchFlags);

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

    auto mocsIndex = latestSentStatelessMocsConfig;
    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);

    if (dispatchFlags.l3CacheSettings != L3CachingSettings::NotApplicable) {
        auto l3On = dispatchFlags.l3CacheSettings != L3CachingSettings::l3CacheOff;
        auto l1On = dispatchFlags.l3CacheSettings == L3CachingSettings::l3AndL1On;
        mocsIndex = hwHelper.getMocsIndex(*device.getGmmHelper(), l3On, l1On);
    }

    if (mocsIndex != latestSentStatelessMocsConfig) {
        isStateBaseAddressDirty = true;
        latestSentStatelessMocsConfig = mocsIndex;
    }

    if ((isMultiOsContextCapable() || dispatchFlags.areMultipleSubDevicesInContext) && (dispatchFlags.useGlobalAtomics != lastSentUseGlobalAtomics)) {
        isStateBaseAddressDirty = true;
        lastSentUseGlobalAtomics = dispatchFlags.useGlobalAtomics;
    }

    bool sourceLevelDebuggerActive = device.getSourceLevelDebugger() != nullptr ? true : false;

    auto memoryCompressionState = lastMemoryCompressionState;
    if (dispatchFlags.memoryCompressionState != MemoryCompressionState::NotApplicable) {
        memoryCompressionState = dispatchFlags.memoryCompressionState;
    }
    if (memoryCompressionState != lastMemoryCompressionState) {
        isStateBaseAddressDirty = true;
        lastMemoryCompressionState = memoryCompressionState;
    }

    //Reprogram state base address if required
    if (isStateBaseAddressDirty || sourceLevelDebuggerActive) {
        addPipeControlBeforeStateBaseAddress(commandStreamCSR);
        programAdditionalPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs, true);

        uint64_t newGSHbase = 0;
        GSBAFor32BitProgrammed = false;
        if (is64bit && scratchSpaceController->getScratchSpaceAllocation() && !force32BitAllocations) {
            newGSHbase = scratchSpaceController->calculateNewGSH();
        } else if (is64bit && force32BitAllocations && dispatchFlags.gsba32BitRequired) {
            bool useLocalMemory = scratchSpaceController->getScratchSpaceAllocation() ? scratchSpaceController->getScratchSpaceAllocation()->isAllocatedInLocalMemoryPool() : false;
            newGSHbase = getMemoryManager()->getExternalHeapBaseAddress(rootDeviceIndex, useLocalMemory);
            GSBAFor32BitProgrammed = true;
        }

        auto stateBaseAddressCmdOffset = commandStreamCSR.getUsed();
        auto pCmd = static_cast<STATE_BASE_ADDRESS *>(commandStreamCSR.getSpace(sizeof(STATE_BASE_ADDRESS)));
        STATE_BASE_ADDRESS cmd;
        auto instructionHeapBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));
        StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
            &cmd,
            &dsh,
            &ioh,
            &ssh,
            newGSHbase,
            true,
            mocsIndex,
            getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, ioh.getGraphicsAllocation()->isAllocatedInLocalMemoryPool()),
            instructionHeapBaseAddress,
            0,
            true,
            false,
            device.getGmmHelper(),
            isMultiOsContextCapable(),
            memoryCompressionState,
            dispatchFlags.useGlobalAtomics,
            dispatchFlags.areMultipleSubDevicesInContext);
        *pCmd = cmd;

        programAdditionalStateBaseAddress(commandStreamCSR, cmd, device);

        if (sshDirty) {
            bindingTableBaseAddressRequired = true;
        }

        if (bindingTableBaseAddressRequired) {
            StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(commandStreamCSR, ssh, device.getGmmHelper());
            bindingTableBaseAddressRequired = false;
        }

        programAdditionalPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs, false);
        programStateSip(commandStreamCSR, device);

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            collectStateBaseAddresPatchInfo(commandStream.getGraphicsAllocation()->getGpuAddress(), stateBaseAddressCmdOffset, dsh, ioh, ssh, newGSHbase);
        }
    }

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskLevel", (uint32_t)this->taskLevel);

    if (executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
        if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
            PipeControlArgs args;
            args.textureCacheInvalidationEnable = true;
            addPipeControlCmd(commandStreamCSR, args);
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
        PipeControlArgs args;
        args.instructionCacheInvalidateEnable = true;
        MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStreamCSR, args);
        requiresInstructionCacheFlush = false;
    }

    // Add a Pipe Control if we have a dependency on a previous walker to avoid concurrency issues.
    if (taskLevel > this->taskLevel) {
        if (!timestampPacketWriteEnabled) {
            PipeControlArgs args;
            MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStreamCSR, args);
        }
        this->taskLevel = taskLevel;
        DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskCount", peekTaskCount());
    }

    if (DebugManager.flags.ForcePipeControlPriorToWalker.get()) {
        forcePipeControl(commandStreamCSR);
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

    if (dispatchFlags.preemptionMode == PreemptionMode::MidThread || sourceLevelDebuggerActive) {
        makeResident(*SipKernel::getSipKernel(device).getSipAllocation());
        if (debugSurface) {
            makeResident(*debugSurface);
        }
    }

    if (experimentalCmdBuffer.get() != nullptr) {
        experimentalCmdBuffer->makeResidentAllocations();
    }

    if (workPartitionAllocation) {
        makeResident(*workPartitionAllocation);
    }

    // If the CSR has work in its CS, flush it before the task
    bool submitTask = commandStreamStartTask != commandStreamTask.getUsed();
    bool submitCSR = (commandStreamStartCSR != commandStreamCSR.getUsed()) || this->isMultiOsContextCapable();
    bool submitCommandStreamFromCsr = false;
    void *bbEndLocation = nullptr;
    auto bbEndPaddingSize = this->dispatchMode == DispatchMode::ImmediateDispatch ? 0 : sizeof(MI_BATCH_BUFFER_START) - sizeof(MI_BATCH_BUFFER_END);
    size_t chainedBatchBufferStartOffset = 0;
    GraphicsAllocation *chainedBatchBuffer = nullptr;
    bool directSubmissionEnabled = isDirectSubmissionEnabled();
    if (submitTask) {
        programEndingCmd(commandStreamTask, device, &bbEndLocation, directSubmissionEnabled);
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
        this->programEpilogue(commandStreamCSR, device, &bbEndLocation, dispatchFlags);

    } else if (submitCSR) {
        programEndingCmd(commandStreamCSR, device, &bbEndLocation, directSubmissionEnabled);
        this->emitNoop(commandStreamCSR, bbEndPaddingSize);
        this->alignToCacheLine(commandStreamCSR);
        DEBUG_BREAK_IF(commandStreamCSR.getUsed() > commandStreamCSR.getMaxAvailableSpace());
        submitCommandStreamFromCsr = true;
    }

    size_t startOffset = submitCommandStreamFromCsr ? commandStreamStartCSR : commandStreamStartTask;
    auto &streamToSubmit = submitCommandStreamFromCsr ? commandStreamCSR : commandStreamTask;
    BatchBuffer batchBuffer{streamToSubmit.getGraphicsAllocation(), startOffset, chainedBatchBufferStartOffset, chainedBatchBuffer,
                            dispatchFlags.requiresCoherency, dispatchFlags.lowPriority, dispatchFlags.throttle, dispatchFlags.sliceCount,
                            streamToSubmit.getUsed(), &streamToSubmit, bbEndLocation, dispatchFlags.useSingleSubdevice};
    streamToSubmit.getGraphicsAllocation()->updateTaskCount(this->taskCount + 1, this->osContext->getContextId());
    streamToSubmit.getGraphicsAllocation()->updateResidencyTaskCount(this->taskCount + 1, this->osContext->getContextId());

    if (submitCSR | submitTask) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            flushHandler(batchBuffer, this->getResidencyAllocations());
            if (updateTag) {
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
            this->submissionAggregator->recordCommandBuffer(commandBuffer);
        }
    } else {
        this->makeSurfacePackNonResident(this->getResidencyAllocations());
    }

    this->wasSubmittedToSingleSubdevice = dispatchFlags.useSingleSubdevice;

    //check if we are not over the budget, if we are do implicit flush
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

    if (this->dispatchMode == DispatchMode::BatchedDispatch && implicitFlush) {
        this->flushBatchedSubmissions();
    }

    ++taskCount;
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskCount", peekTaskCount());
    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "Current taskCount:", tagAddress ? *tagAddress : 0);

    CompletionStamp completionStamp = {
        taskCount,
        this->taskLevel,
        flushStamp->peekStamp()};

    this->taskLevel += levelClosed ? 1 : 0;

    return completionStamp;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::forcePipeControl(NEO::LinearStream &commandStreamCSR) {
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControlWithCSStallOnly(commandStreamCSR);
    MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStreamCSR, args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPipeControlForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags) {
    stallingPipeControlOnNextFlushRequired = false;

    auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;

    if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() != 0) {
        auto barrierTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*dispatchFlags.barrierTimestampPacketNodes->peekNodes()[0]);

        PipeControlArgs args(true);
        MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
            cmdStream,
            PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
            barrierTimestampPacketGpuAddress,
            0,
            peekHwInfo(),
            args);

        dispatchFlags.barrierTimestampPacketNodes->makeResident(*this);
    } else {
        PipeControlArgs args;
        MemorySynchronizationCommands<GfxFamily>::addPipeControl(cmdStream, args);
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
        auto pipeControlLocationSize = MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo());
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
                auto cpuAddressForCommandBufferDestination = ptrOffset(nextCommandBuffer->batchBuffer.commandBufferAllocation->getUnderlyingBuffer(), nextCommandBuffer->batchBuffer.startOffset);
                auto cpuAddressForCurrentCommandBufferEndingSection = alignUp(ptrOffset(currentBBendLocation, sizeof(MI_BATCH_BUFFER_START)), MemoryConstants::cacheLineSize);

                //if we point to exact same command buffer, then batch buffer start is not needed at all
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
                nextCommandBuffer = nextCommandBuffer->next;

                commandBufferList.removeFrontOne();
            }
            surfacesForSubmit.reserve(resourcePackage.size() + 1);
            for (auto &surface : resourcePackage) {
                surfacesForSubmit.push_back(surface);
            }

            //make sure we flush DC if needed
            if (epiloguePipeControlLocation) {
                bool flushDcInEpilogue = MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed();

                if (DebugManager.flags.DisableDcFlushInEpilogue.get()) {
                    flushDcInEpilogue = false;
                }
                ((PIPE_CONTROL *)epiloguePipeControlLocation)->setDcFlushEnable(flushDcInEpilogue);
            }

            primaryCmdBuffer->batchBuffer.endCmdPtr = currentBBendLocation;

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
    size += getRequiredStateBaseAddressSize(device);
    if (!this->isStateSipSent || device.isDebuggerActive()) {
        size += PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(device);
    }
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl();
    size += sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);

    size += getCmdSizeForL3Config();
    size += getCmdSizeForComputeMode();
    size += getCmdSizeForMediaSampler(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    size += getCmdSizeForPipelineSelect();
    size += getCmdSizeForPreemption(dispatchFlags);
    size += getCmdSizeForPerDssBackedBuffer(device.getHardwareInfo());
    size += getCmdSizeForEpilogue(dispatchFlags);
    size += getCmdsSizeForHardwareContext();

    if (executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
        if (this->samplerCacheFlushRequired != SamplerCacheFlushState::samplerCacheFlushNotRequired) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }
    if (experimentalCmdBuffer.get() != nullptr) {
        size += experimentalCmdBuffer->getRequiredInjectionSize<GfxFamily>();
    }

    size += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.csrDependencies);
    size += TimestampPacketHelper::getRequiredCmdStreamSizeForTaskCountContainer<GfxFamily>(dispatchFlags.csrDependencies);

    if (stallingPipeControlOnNextFlushRequired) {
        auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;
        if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() > 0) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo());
        } else {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }

    if (requiresInstructionCacheFlush) {
        size += sizeof(typename GfxFamily::PIPE_CONTROL);
    }

    if (DebugManager.flags.ForcePipeControlPriorToWalker.get()) {
        size += 2 * sizeof(PIPE_CONTROL);
    }

    return size;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPipelineSelect() const {

    size_t size = 0;
    if ((csrSizeRequestFlags.mediaSamplerConfigChanged ||
         csrSizeRequestFlags.specialPipelineSelectModeChanged ||
         !isPreambleSent) &&
        !isPipelineSelectAlreadyProgrammed()) {
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
    updateTagFromWait();

    int64_t waitTimeout = 0;
    bool enableTimeout = false;

    enableTimeout = kmdNotifyHelper->obtainTimeoutParams(waitTimeout, useQuickKmdSleep, *getTagAddress(), taskCountToWait, flushStampToWait, forcePowerSavingMode, this->isKmdWaitModeActive());

    PRINT_DEBUG_STRING(DebugManager.flags.LogWaitingForCompletion.get(), stdout,
                       "\nWaiting for task count %u at location %p. Current value: %u\n",
                       taskCountToWait, getTagAddress(), *getTagAddress());

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

    PRINT_DEBUG_STRING(DebugManager.flags.LogWaitingForCompletion.get(), stdout,
                       "\nWaiting completed. Current value: %u\n", *getTagAddress());
}

template <typename GfxFamily>
inline const HardwareInfo &CommandStreamReceiverHw<GfxFamily>::peekHwInfo() const {
    return *executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo();
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
        PreambleHelper<GfxFamily>::programPreamble(&csr, device, newL3Config, this->requiredThreadArbitrationPolicy, this->preemptionAllocation);
        this->isPreambleSent = true;
        this->lastSentL3Config = newL3Config;
        this->lastSentThreadArbitrationPolicy = this->requiredThreadArbitrationPolicy;
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
        auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        auto engineGroupType = hwHelper.getEngineGroupType(getOsContext().getEngineType(), hwInfo);
        auto pVfeState = PreambleHelper<GfxFamily>::getSpaceForVfeState(&csr, hwInfo, engineGroupType);
        auto disableOverdispatch = hwHelper.isDisableOverdispatchAvailable(hwInfo) &&
                                   (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotSet);
        StreamProperties streamProperties{};
        streamProperties.frontEndState.setProperties(lastKernelExecutionType == KernelExecutionType::Concurrent,
                                                     disableOverdispatch, hwInfo);
        PreambleHelper<GfxFamily>::programVfeState(
            pVfeState, hwInfo, requiredScratchSize, getScratchPatchAddress(),
            maxFrontEndThreads, lastAdditionalKernelExecInfo, streamProperties);
        auto commandOffset = PreambleHelper<GfxFamily>::getScratchSpaceAddressOffsetForVfeState(&csr, pVfeState);

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
uint32_t CommandStreamReceiverHw<GfxFamily>::blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    auto lock = obtainUniqueOwnership();
    bool blitterDirectSubmission = this->isBlitterDirectSubmissionEnabled();
    auto &commandStream = getCS(BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitPropertiesContainer, profilingEnabled, PauseOnGpuProperties::featureEnabled(DebugManager.flags.PauseOnBlitCopy.get()),
                                                                                        blitterDirectSubmission, *this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]));
    auto commandStreamStart = commandStream.getUsed();
    auto newTaskCount = taskCount + 1;
    latestSentTaskCount = newTaskCount;

    getOsContext().ensureContextInitialized();
    this->initDirectSubmission(device, getOsContext());

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(), DebugPauseState::waitingForUserStartConfirmation, DebugPauseState::hasUserStartConfirmation);
    }

    programEnginePrologue(commandStream);

    for (auto &blitProperties : blitPropertiesContainer) {
        TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);
        TimestampPacketHelper::programCsrDependenciesForForTaskCountContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);

        if (blitProperties.outputTimestampPacket && profilingEnabled) {
            BlitCommandsHelper<GfxFamily>::encodeProfilingStartMmios(commandStream, *blitProperties.outputTimestampPacket);
        }

        BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(blitProperties, commandStream, *this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]);

        if (blitProperties.outputTimestampPacket) {
            if (profilingEnabled) {
                MiFlushArgs args;
                EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, 0llu, newTaskCount, args);

                BlitCommandsHelper<GfxFamily>::encodeProfilingEndMmios(commandStream, *blitProperties.outputTimestampPacket);
            } else {
                auto timestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*blitProperties.outputTimestampPacket);
                MiFlushArgs args;
                args.commandWithPostSync = true;
                EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, timestampPacketGpuAddress, 0, args);
            }
            makeResident(*blitProperties.outputTimestampPacket->getBaseGraphicsAllocation());
        }

        blitProperties.csrDependencies.makeResident(*this);

        makeResident(*blitProperties.srcAllocation);
        makeResident(*blitProperties.dstAllocation);
        if (blitProperties.clearColorAllocation) {
            makeResident(*blitProperties.clearColorAllocation);
        }
    }

    BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(commandStream);

    MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), peekHwInfo());

    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, tagAllocation->getGpuAddress(), newTaskCount, args);

    MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), peekHwInfo());

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(), DebugPauseState::waitingForUserEndConfirmation, DebugPauseState::hasUserEndConfirmation);
    }

    void *endingCmdPtr = nullptr;
    if (blitterDirectSubmission) {
        endingCmdPtr = commandStream.getSpace(0);
        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStream,
                                                                        0ull,
                                                                        false);
    } else {
        auto batchBufferEnd = reinterpret_cast<MI_BATCH_BUFFER_END *>(
            commandStream.getSpace(sizeof(MI_BATCH_BUFFER_END)));
        *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;
    }

    alignToCacheLine(commandStream);

    makeResident(*tagAllocation);
    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    BatchBuffer batchBuffer{commandStream.getGraphicsAllocation(), commandStreamStart, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStream.getUsed(), &commandStream, endingCmdPtr, false};

    commandStream.getGraphicsAllocation()->updateTaskCount(newTaskCount, this->osContext->getContextId());
    commandStream.getGraphicsAllocation()->updateResidencyTaskCount(newTaskCount, this->osContext->getContextId());

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
inline void CommandStreamReceiverHw<GfxFamily>::flushTagUpdate() {
    if (this->osContext != nullptr) {
        if (this->osContext->getEngineType() == aub_stream::ENGINE_BCS) {
            this->flushMiFlushDW();
        } else {
            this->flushPipeControl();
        }
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushNonKernelTask(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isWaitOnEvent, bool isStartOfDispatch, bool isEndOfDispatch) {
    if (isWaitOnEvent) {
        this->flushSemaphoreWait(eventAlloc, immediateGpuAddress, immediateData, args, isStartOfDispatch, isEndOfDispatch);
    } else {
        if (this->osContext->getEngineType() == aub_stream::ENGINE_BCS) {
            this->flushMiFlushDW(eventAlloc, immediateGpuAddress, immediateData);
        } else {
            this->flushPipeControl(eventAlloc, immediateGpuAddress, immediateData, args);
        }
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::flushMiFlushDW() {
    auto lock = obtainUniqueOwnership();

    auto &commandStream = getCS(EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite());
    auto commandStreamStart = commandStream.getUsed();

    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, tagAllocation->getGpuAddress(), taskCount, args);

    makeResident(*tagAllocation);

    this->flushSmallTask(commandStream, commandStreamStart);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushMiFlushDW(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData) {
    auto lock = obtainUniqueOwnership();

    auto &commandStream = getCS(EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite());
    auto commandStreamStart = commandStream.getUsed();

    programHardwareContext(commandStream);

    MiFlushArgs args;
    if (eventAlloc) {
        args.commandWithPostSync = true;
        EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, immediateGpuAddress, immediateData, args);
        makeResident(*eventAlloc);
    } else {
        EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, 0, 0, args);
    }

    this->flushSmallTask(commandStream, commandStreamStart);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushPipeControl() {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    auto lock = obtainUniqueOwnership();

    auto &commandStream = getCS(MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo()));
    auto commandStreamStart = commandStream.getUsed();

    PipeControlArgs args(true);
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(commandStream,
                                                                                        PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                                                                                        getTagAllocation()->getGpuAddress(),
                                                                                        taskCount + 1,
                                                                                        peekHwInfo(),
                                                                                        args);

    makeResident(*tagAllocation);

    this->flushSmallTask(commandStream, commandStreamStart);

    this->latestFlushedTaskCount = taskCount + 1;
    this->latestSentTaskCount = taskCount + 1;
    taskCount++;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushPipeControl(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    auto lock = obtainUniqueOwnership();
    auto &commandStream = getCS(MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl());
    auto commandStreamStart = commandStream.getUsed();

    programHardwareContext(commandStream);

    if (eventAlloc) {
        MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(commandStream,
                                                                                            PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                                                                                            immediateGpuAddress,
                                                                                            immediateData,
                                                                                            peekHwInfo(),
                                                                                            args);
        makeResident(*eventAlloc);
    } else {
        NEO::PipeControlArgs args;
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);
    }

    this->flushSmallTask(commandStream, commandStreamStart);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushSemaphoreWait(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isStartOfDispatch, bool isEndOfDispatch) {
    auto lock = obtainUniqueOwnership();
    if (isStartOfDispatch && args.dcFlushEnable) {
        if (this->osContext->getEngineType() == aub_stream::ENGINE_BCS) {
            LinearStream &commandStream = getCS(EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite());
            cmdStreamStart = commandStream.getUsed();
            MiFlushArgs args;
            EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, 0, 0, args);
        } else {
            LinearStream &commandStream = getCS(MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl());
            cmdStreamStart = commandStream.getUsed();
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);
        }
    }

    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    LinearStream &commandStream = getCS(NEO::EncodeSempahore<GfxFamily>::getSizeMiSemaphoreWait());
    if (isStartOfDispatch && !args.dcFlushEnable) {
        cmdStreamStart = commandStream.getUsed();
    }

    programHardwareContext(commandStream);

    NEO::EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(commandStream,
                                                               immediateGpuAddress,
                                                               static_cast<uint32_t>(immediateData),
                                                               MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);

    makeResident(*eventAlloc);

    if (isEndOfDispatch) {
        this->flushSmallTask(commandStream, cmdStreamStart);
        cmdStreamStart = 0;
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushSmallTask(LinearStream &commandStreamTask, size_t commandStreamStartTask) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    void *endingCmdPtr = nullptr;

    if (isDirectSubmissionEnabled()) {
        endingCmdPtr = commandStreamTask.getSpace(0);
        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStreamTask,
                                                                        0ull,
                                                                        false);
    } else {
        auto batchBufferEnd = reinterpret_cast<MI_BATCH_BUFFER_END *>(commandStreamTask.getSpace(sizeof(MI_BATCH_BUFFER_END)));
        *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;
    }

    alignToCacheLine(commandStreamTask);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    BatchBuffer batchBuffer{commandStreamTask.getGraphicsAllocation(), commandStreamStartTask, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStreamTask.getUsed(), &commandStreamTask, endingCmdPtr, false};

    flushHandler(batchBuffer, getResidencyAllocations());
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    flush(batchBuffer, allocationsForResidency);
    makeSurfacePackNonResident(allocationsForResidency);
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isUpdateTagFromWaitEnabled() {
    bool enabled = false;

    if (DebugManager.flags.UpdateTaskCountFromWait.get() != -1) {
        enabled = DebugManager.flags.UpdateTaskCountFromWait.get();
    }

    return enabled;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::updateTagFromWait() {
    if (isUpdateTagFromWaitEnabled()) {
        flushTagUpdate();
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programAdditionalPipelineSelect(LinearStream &csr, PipelineSelectArgs &pipelineSelectArgs, bool is3DPipeline) {
    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    if (hwHelper.is3DPipelineSelectWARequired(peekHwInfo()) && isRcs()) {
        auto localPipelineSelectArgs = pipelineSelectArgs;
        localPipelineSelectArgs.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<GfxFamily>::programPipelineSelect(&csr, localPipelineSelectArgs, peekHwInfo());
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programAdditionalStateBaseAddress(LinearStream &csr, typename GfxFamily::STATE_BASE_ADDRESS &cmd, Device &device) {}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isComputeModeNeeded() const {
    return false;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isAdditionalPipeControlNeeded() const {
    return false;
}

template <typename GfxFamily>
inline MemoryCompressionState CommandStreamReceiverHw<GfxFamily>::getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const {
    return MemoryCompressionState::NotApplicable;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isPipelineSelectAlreadyProgrammed() const {
    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    return isComputeModeNeeded() && hwHelper.is3DPipelineSelectWARequired(peekHwInfo()) && isRcs();
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEpilogue(LinearStream &csr, Device &device, void **batchBufferEndLocation, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.epilogueRequired) {
        auto currentOffset = ptrDiff(csr.getSpace(0u), csr.getCpuBase());
        auto gpuAddress = ptrOffset(csr.getGraphicsAllocation()->getGpuAddress(), currentOffset);

        addBatchBufferStart(reinterpret_cast<typename GfxFamily::MI_BATCH_BUFFER_START *>(*batchBufferEndLocation), gpuAddress, false);
        this->programEpliogueCommands(csr, dispatchFlags);
        programEndingCmd(csr, device, batchBufferEndLocation, isDirectSubmissionEnabled());
        this->alignToCacheLine(csr);
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
inline bool CommandStreamReceiverHw<GfxFamily>::initDirectSubmission(Device &device, OsContext &osContext) {
    bool ret = true;

    bool submitOnInit = false;
    auto startDirect = osContext.isDirectSubmissionAvailable(device.getHardwareInfo(), submitOnInit);

    if (startDirect) {
        if (EngineHelpers::isBcs(osContext.getEngineType())) {
            if (!this->isBlitterDirectSubmissionEnabled()) {
                blitterDirectSubmission = DirectSubmissionHw<GfxFamily, BlitterDispatcher<GfxFamily>>::create(device, osContext);
                ret = blitterDirectSubmission->initialize(submitOnInit);
            }
        } else {
            if (!this->isDirectSubmissionEnabled()) {
                directSubmission = DirectSubmissionHw<GfxFamily, RenderDispatcher<GfxFamily>>::create(device, osContext);
                ret = directSubmission->initialize(submitOnInit);
            }
        }
        osContext.setDirectSubmissionActive();
    }
    return ret;
}

template <typename GfxFamily>
TagAllocatorBase *CommandStreamReceiverHw<GfxFamily>::getTimestampPacketAllocator() {
    if (timestampPacketAllocator.get() == nullptr) {
        auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
        const std::vector<uint32_t> rootDeviceIndices = {rootDeviceIndex};

        timestampPacketAllocator = hwHelper.createTimestampPacketAllocator(rootDeviceIndices, getMemoryManager(), getPreferredTagPoolSize(), getType(), osContext->getDeviceBitfield());
    }
    return timestampPacketAllocator.get();
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

} // namespace NEO
