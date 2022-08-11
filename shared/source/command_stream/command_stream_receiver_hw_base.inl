/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/scratch_space_controller_base.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/direct_submission_controller.h"
#include "shared/source/direct_submission/direct_submission_hw.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/logical_state_helper.h"
#include "shared/source/helpers/pause_on_gpu_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_context.h"
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
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    localMemoryEnabled = hwHelper.getEnableLocalMemory(hwInfo);

    resetKmdNotifyHelper(new KmdNotifyHelper(&hwInfo.capabilityTable.kmdNotifyProperties));

    if (DebugManager.flags.FlattenBatchBufferForAUBDump.get() || DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
        flatBatchBufferHelper.reset(new FlatBatchBufferHelperHw<GfxFamily>(executionEnvironment));
    }
    defaultSshSize = getSshHeapSize();
    canUse4GbHeaps = are4GbHeapsAvailable();

    timestampPacketWriteEnabled = hwHelper.timestampPacketWriteSupported();
    if (DebugManager.flags.EnableTimestampPacket.get() != -1) {
        timestampPacketWriteEnabled = !!DebugManager.flags.EnableTimestampPacket.get();
    }

    logicalStateHelper.reset(LogicalStateHelper::create<GfxFamily>());

    createScratchSpaceController();
    configurePostSyncWriteOffset();
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
inline void CommandStreamReceiverHw<GfxFamily>::programEndingCmd(LinearStream &commandStream, Device &device, void **patchLocation, bool directSubmissionEnabled) {
    if (directSubmissionEnabled) {
        uint64_t startAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed();
        if (DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
            startAddress = 0;
        }

        *patchLocation = commandStream.getSpace(sizeof(MI_BATCH_BUFFER_START));
        auto bbStart = reinterpret_cast<MI_BATCH_BUFFER_START *>(*patchLocation);
        MI_BATCH_BUFFER_START cmd = {};

        addBatchBufferStart(&cmd, startAddress, false);
        *bbStart = cmd;
    } else {
        if (!EngineHelpers::isBcs(osContext->getEngineType())) {
            PreemptionHelper::programStateSipEndWa<GfxFamily>(commandStream, device);
        }
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
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTask(
    LinearStream &commandStreamTask,
    size_t commandStreamStartTask,
    const IndirectHeap *dsh,
    const IndirectHeap *ioh,
    const IndirectHeap *ssh,
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
    PipeControlArgs args;

    bool csrFlush = this->wasSubmittedToSingleSubdevice != dispatchFlags.useSingleSubdevice;

    csrFlush |= DebugManager.flags.ForceCsrFlushing.get();

    if (csrFlush) {
        flushBatchedSubmissions();
    }

    if (detectInitProgrammingFlagsRequired(dispatchFlags)) {
        initProgrammingFlags();
    }

    const auto &hwInfo = peekHwInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    if (dispatchFlags.blocking || dispatchFlags.dcFlush || dispatchFlags.guardCommandBufferWithPipeControl) {
        if (this->dispatchMode == DispatchMode::ImmediateDispatch) {
            // for ImmediateDispatch we will send this right away, therefore this pipe control will close the level
            // for BatchedSubmissions it will be nooped and only last ppc in batch will be emitted.
            levelClosed = true;
            // if we guard with ppc, flush dc as well to speed up completion latency
            if (dispatchFlags.guardCommandBufferWithPipeControl) {
                const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
                if (hwInfoConfig.isDcFlushAllowed()) {
                    dispatchFlags.dcFlush = true;
                }
            }
        }

        epiloguePipeControlLocation = ptrOffset(commandStreamTask.getCpuBase(), commandStreamTask.getUsed());

        if ((dispatchFlags.outOfOrderExecutionAllowed || timestampPacketWriteEnabled) &&
            !dispatchFlags.dcFlush) {
            currentPipeControlForNooping = epiloguePipeControlLocation;
        }

        auto address = getTagAllocation()->getGpuAddress();

        args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(dispatchFlags.dcFlush, hwInfo);
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        args.tlbInvalidation |= dispatchFlags.memoryMigrationRequired;
        args.textureCacheInvalidationEnable |= dispatchFlags.textureCacheFlush;
        args.workloadPartitionOffset = isMultiTileOperationEnabled();
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            commandStreamTask,
            PostSyncMode::ImmediateData,
            address,
            taskCount + 1,
            hwInfo,
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
    auto isSpecialPipelineSelectModeChanged = PreambleHelper<GfxFamily>::isSpecialPipelineSelectModeChanged(lastSpecialPipelineSelectMode,
                                                                                                            dispatchFlags.pipelineSelectArgs.specialPipelineSelectMode,
                                                                                                            hwInfo);

    auto requiresCoherency = hwHelper.forceNonGpuCoherencyWA(dispatchFlags.requiresCoherency);
    this->streamProperties.stateComputeMode.setProperties(requiresCoherency, dispatchFlags.numGrfRequired,
                                                          dispatchFlags.threadArbitrationPolicy, device.getPreemptionMode(), hwInfo);

    csrSizeRequestFlags.l3ConfigChanged = this->lastSentL3Config != newL3Config;
    csrSizeRequestFlags.preemptionRequestChanged = this->lastPreemptionMode != dispatchFlags.preemptionMode;
    csrSizeRequestFlags.mediaSamplerConfigChanged = this->lastMediaSamplerConfig != static_cast<int8_t>(dispatchFlags.pipelineSelectArgs.mediaSamplerRequired);
    csrSizeRequestFlags.specialPipelineSelectModeChanged = isSpecialPipelineSelectModeChanged;

    csrSizeRequestFlags.activePartitionsChanged = isProgramActivePartitionConfigRequired();

    auto force32BitAllocations = getMemoryManager()->peekForce32BitAllocations();
    bool stateBaseAddressDirty = false;

    bool checkVfeStateDirty = false;
    if (requiredScratchSize || requiredPrivateScratchSize) {
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

    if (!logicalStateHelper) {
        if (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotApplicable && lastAdditionalKernelExecInfo != dispatchFlags.additionalKernelExecInfo) {
            setMediaVFEStateDirty(true);
        }

        if (dispatchFlags.kernelExecutionType != KernelExecutionType::NotApplicable && lastKernelExecutionType != dispatchFlags.kernelExecutionType) {
            setMediaVFEStateDirty(true);
        }
    }

    auto &commandStreamCSR = this->getCS(getRequiredCmdStreamSizeAligned(dispatchFlags, device));
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);
    TimestampPacketHelper::programCsrDependenciesForForTaskCountContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies);

    programActivePartitionConfigFlushTask(commandStreamCSR);
    programEngineModeCommands(commandStreamCSR, dispatchFlags);

    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = pageTableManager->initPageTableManagerRegisters(this);
    }

    programHardwareContext(commandStreamCSR);
    programComputeMode(commandStreamCSR, dispatchFlags, hwInfo);
    programPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs);
    programL3(commandStreamCSR, newL3Config);
    programPreamble(commandStreamCSR, device, newL3Config);
    programMediaSampler(commandStreamCSR, dispatchFlags);
    addPipeControlBefore3dState(commandStreamCSR, dispatchFlags);
    programPerDssBackedBuffer(commandStreamCSR, device, dispatchFlags);

    stateBaseAddressDirty |= ((GSBAFor32BitProgrammed ^ dispatchFlags.gsba32BitRequired) && force32BitAllocations);

    programVFEState(commandStreamCSR, dispatchFlags, device.getDeviceInfo().maxFrontEndThreads);

    programPreemption(commandStreamCSR, dispatchFlags);

    EncodeKernelArgsBuffer<GfxFamily>::encodeKernelArgsBufferCmds(kernelArgsBufferAllocation, logicalStateHelper.get());

    if (stallingCommandsOnNextFlushRequired) {
        programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags);
    }
    const bool hasDsh = hwInfo.capabilityTable.supportsImages;
    bool dshDirty = hasDsh ? dshState.updateAndCheck(dsh) : false;
    bool iohDirty = iohState.updateAndCheck(ioh);
    bool sshDirty = sshState.updateAndCheck(ssh);

    auto isStateBaseAddressDirty = dshDirty || iohDirty || sshDirty || stateBaseAddressDirty;

    auto mocsIndex = latestSentStatelessMocsConfig;

    if (dispatchFlags.l3CacheSettings != L3CachingSettings::NotApplicable) {
        auto l3On = dispatchFlags.l3CacheSettings != L3CachingSettings::l3CacheOff;
        auto l1On = dispatchFlags.l3CacheSettings == L3CachingSettings::l3AndL1On;
        mocsIndex = hwHelper.getMocsIndex(*device.getGmmHelper(), l3On, l1On);
    }

    if (mocsIndex != latestSentStatelessMocsConfig) {
        isStateBaseAddressDirty = true;
        latestSentStatelessMocsConfig = mocsIndex;
    }

    if (this->isGlobalAtomicsProgrammingRequired(dispatchFlags.useGlobalAtomics) && (this->isMultiOsContextCapable() || dispatchFlags.areMultipleSubDevicesInContext)) {
        isStateBaseAddressDirty = true;
        lastSentUseGlobalAtomics = dispatchFlags.useGlobalAtomics;
    }

    bool debuggingEnabled = device.getDebugger() != nullptr;
    bool sourceLevelDebuggerActive = device.getSourceLevelDebugger() != nullptr ? true : false;

    auto memoryCompressionState = lastMemoryCompressionState;
    if (dispatchFlags.memoryCompressionState != MemoryCompressionState::NotApplicable) {
        memoryCompressionState = dispatchFlags.memoryCompressionState;
    }
    if (memoryCompressionState != lastMemoryCompressionState) {
        isStateBaseAddressDirty = true;
        lastMemoryCompressionState = memoryCompressionState;
    }

    // Reprogram state base address if required
    if (isStateBaseAddressDirty || sourceLevelDebuggerActive) {
        EncodeWA<GfxFamily>::addPipeControlBeforeStateBaseAddress(commandStreamCSR, hwInfo, isRcs());
        EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs, true, hwInfo, isRcs());

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
        auto stateBaseAddressCmdBuffer = StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStreamCSR);
        auto instructionHeapBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, getMemoryManager()->isLocalMemoryUsedForIsa(rootDeviceIndex));
        uint64_t indirectObjectStateBaseAddress = getMemoryManager()->getInternalHeapBaseAddress(rootDeviceIndex, ioh->getGraphicsAllocation()->isAllocatedInLocalMemoryPool());

        STATE_BASE_ADDRESS stateBaseAddressCmd;

        StateBaseAddressHelperArgs<GfxFamily> args = {
            newGSHbase,                                  // generalStateBase
            indirectObjectStateBaseAddress,              // indirectObjectHeapBaseAddress
            instructionHeapBaseAddress,                  // instructionHeapBaseAddress
            0,                                           // globalHeapsBaseAddress
            &stateBaseAddressCmd,                        // stateBaseAddressCmd
            dsh,                                         // dsh
            ioh,                                         // ioh
            ssh,                                         // ssh
            device.getGmmHelper(),                       // gmmHelper
            mocsIndex,                                   // statelessMocsIndex
            memoryCompressionState,                      // memoryCompressionState
            true,                                        // setInstructionStateBaseAddress
            true,                                        // setGeneralStateBaseAddress
            false,                                       // useGlobalHeapsBaseAddress
            isMultiOsContextCapable(),                   // isMultiOsContextCapable
            dispatchFlags.useGlobalAtomics,              // useGlobalAtomics
            dispatchFlags.areMultipleSubDevicesInContext // areMultipleSubDevicesInContext
        };

        StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(args);

        if (stateBaseAddressCmdBuffer) {
            *stateBaseAddressCmdBuffer = stateBaseAddressCmd;
        }

        programAdditionalStateBaseAddress(commandStreamCSR, stateBaseAddressCmd, device);

        if (debuggingEnabled && !device.getDebugger()->isLegacy()) {
            NEO::Debugger::SbaAddresses sbaAddresses = {};
            NEO::EncodeStateBaseAddress<GfxFamily>::setSbaAddressesForDebugger(sbaAddresses, stateBaseAddressCmd);
            device.getDebugger()->captureStateBaseAddress(commandStreamCSR, sbaAddresses);
        }

        if (sshDirty) {
            bindingTableBaseAddressRequired = true;
        }

        if (bindingTableBaseAddressRequired) {
            StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(commandStreamCSR, *ssh, device.getGmmHelper());
            bindingTableBaseAddressRequired = false;
        }

        EncodeWA<GfxFamily>::encodeAdditionalPipelineSelect(commandStreamCSR, dispatchFlags.pipelineSelectArgs, false, hwInfo, isRcs());

        if (DebugManager.flags.AddPatchInfoCommentsForAUBDump.get()) {
            collectStateBaseAddresPatchInfo(commandStream.getGraphicsAllocation()->getGpuAddress(), stateBaseAddressCmdOffset, dsh, ioh, ssh, newGSHbase);
        }
    }

    addPipeControlBeforeStateSip(commandStreamCSR, device);
    programStateSip(commandStreamCSR, device);

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskLevel", (uint32_t)this->taskLevel);

    if (executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads) {
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

    if (experimentalCmdBuffer.get() != nullptr) {
        size_t startingOffset = experimentalCmdBuffer->programExperimentalCommandBuffer<GfxFamily>();
        experimentalCmdBuffer->injectBufferStart<GfxFamily>(commandStreamCSR, startingOffset);
    }

    if (requiresInstructionCacheFlush) {
        PipeControlArgs args;
        args.instructionCacheInvalidateEnable = true;
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(commandStreamCSR, args);
        requiresInstructionCacheFlush = false;
    }

    // Add a Pipe Control if we have a dependency on a previous walker to avoid concurrency issues.
    if (taskLevel > this->taskLevel) {
        auto programPipeControl = !timestampPacketWriteEnabled;

        if (DebugManager.flags.ResolveDependenciesViaPipeControls.get() == 1) {
            programPipeControl = true;
        }

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
    if (hasDsh) {
        auto dshAllocation = dsh->getGraphicsAllocation();
        this->makeResident(*dshAllocation);
        dshAllocation->setEvictable(false);
    }
    auto iohAllocation = ioh->getGraphicsAllocation();
    auto sshAllocation = ssh->getGraphicsAllocation();

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

    if (dispatchFlags.preemptionMode == PreemptionMode::MidThread || debuggingEnabled) {
        makeResident(*SipKernel::getSipKernel(device).getSipAllocation());
    }

    if (sourceLevelDebuggerActive && debugSurface) {
        makeResident(*debugSurface);
    }

    if (experimentalCmdBuffer.get() != nullptr) {
        experimentalCmdBuffer->makeResidentAllocations();
    }

    if (workPartitionAllocation) {
        makeResident(*workPartitionAllocation);
    }

    if (kernelArgsBufferAllocation) {
        makeResident(*kernelArgsBufferAllocation);
    }

    if (logicalStateHelper) {
        logicalStateHelper->writeStreamInline(commandStreamCSR, false);
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
        programEndingCmd(commandStreamCSR, device, &bbEndLocation, directSubmissionEnabled);
        EncodeNoop<GfxFamily>::emitNoop(commandStreamCSR, bbEndPaddingSize);
        EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamCSR);
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
            if (dispatchFlags.blocking || dispatchFlags.dcFlush || dispatchFlags.guardCommandBufferWithPipeControl) {
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

    this->wasSubmittedToSingleSubdevice = dispatchFlags.useSingleSubdevice;

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
            hasSharedHandles(), hwInfo, isRcs(), logicalStateHelper.get());
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingCommandsForBarrier(LinearStream &cmdStream, DispatchFlags &dispatchFlags) {
    stallingCommandsOnNextFlushRequired = false;

    auto barrierTimestampPacketNodes = dispatchFlags.barrierTimestampPacketNodes;

    if (barrierTimestampPacketNodes && barrierTimestampPacketNodes->peekNodes().size() != 0) {
        programStallingPostSyncCommandsForBarrier(cmdStream, *barrierTimestampPacketNodes->peekNodes()[0]);
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
        const auto &hwInfo = peekHwInfo();
        auto pipeControlLocationSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo);
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
                        flatBatchBufferHelper->removePipeControlData(pipeControlLocationSize, currentPipeControlForNooping, hwInfo);
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
            if (epiloguePipeControlLocation && MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo)) {
                lastPipeControlArgs.dcFlushEnable = true;

                if (DebugManager.flags.DisableDcFlushInEpilogue.get()) {
                    lastPipeControlArgs.dcFlushEnable = false;
                }

                MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(
                    epiloguePipeControlLocation,
                    PostSyncMode::ImmediateData,
                    getTagAllocation()->getGpuAddress(),
                    lastTaskCount,
                    hwInfo,
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
    if (!this->isStateSipSent || device.getDebugger()) {
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
    if (dispatchFlags.usePerDssBackedBuffer && !isPerDssBackedBufferSent) {
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

    size += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(dispatchFlags.csrDependencies);
    size += TimestampPacketHelper::getRequiredCmdStreamSizeForTaskCountContainer<GfxFamily>(dispatchFlags.csrDependencies);

    size += EncodeKernelArgsBuffer<GfxFamily>::getKernelArgsBufferCmdsSize(kernelArgsBufferAllocation, logicalStateHelper.get());

    if (stallingCommandsOnNextFlushRequired) {
        size += getCmdSizeForStallingCommands(dispatchFlags);
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
inline WaitStatus CommandStreamReceiverHw<GfxFamily>::waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool useQuickKmdSleep, QueueThrottle throttle) {
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
        UNRECOVERABLE_IF(*(ptrOffset(getTagAddress(), (i * this->postSyncWriteOffset))) < taskCountToWait);
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
    bool debuggingEnabled = device.getDebugger() != nullptr;
    if (!this->isStateSipSent || debuggingEnabled) {
        PreemptionHelper::programStateSip<GfxFamily>(cmdStream, device, logicalStateHelper.get());
        this->isStateSipSent = true;
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programPreamble(LinearStream &csr, Device &device, uint32_t &newL3Config) {
    if (!this->isPreambleSent) {
        PreambleHelper<GfxFamily>::programPreamble(&csr, device, newL3Config, this->preemptionAllocation, logicalStateHelper.get());
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
        auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
        auto engineGroupType = hwHelper.getEngineGroupType(getOsContext().getEngineType(), getOsContext().getEngineUsage(), hwInfo);
        auto pVfeState = PreambleHelper<GfxFamily>::getSpaceForVfeState(&csr, hwInfo, engineGroupType);
        auto disableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(hwInfo) &&
                                   (dispatchFlags.additionalKernelExecInfo != AdditionalKernelExecInfo::NotSet);
        streamProperties.frontEndState.setProperties(lastKernelExecutionType == KernelExecutionType::Concurrent,
                                                     dispatchFlags.disableEUFusion, disableOverdispatch, osContext->isEngineInstanced(), hwInfo);
        PreambleHelper<GfxFamily>::programVfeState(
            pVfeState, hwInfo, requiredScratchSize, getScratchPatchAddress(),
            maxFrontEndThreads, streamProperties, logicalStateHelper.get());
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
    const LinearStream *dsh,
    const LinearStream *ioh,
    const LinearStream *ssh,
    uint64_t generalStateBase) {

    typedef typename GfxFamily::STATE_BASE_ADDRESS STATE_BASE_ADDRESS;
    if constexpr (GfxFamily::supportsSampler) {
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
std::optional<uint32_t> CommandStreamReceiverHw<GfxFamily>::flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
    using MI_FLUSH_DW = typename GfxFamily::MI_FLUSH_DW;

    auto lock = obtainUniqueOwnership();
    bool blitterDirectSubmission = this->isBlitterDirectSubmissionEnabled();
    auto debugPauseEnabled = PauseOnGpuProperties::featureEnabled(DebugManager.flags.PauseOnBlitCopy.get());
    auto &commandStream = getCS(BlitCommandsHelper<GfxFamily>::estimateBlitCommandsSize(blitPropertiesContainer, profilingEnabled, debugPauseEnabled, blitterDirectSubmission,
                                                                                        *this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]));
    auto commandStreamStart = commandStream.getUsed();
    auto newTaskCount = taskCount + 1;
    latestSentTaskCount = newTaskCount;

    getOsContext().ensureContextInitialized();
    this->initDirectSubmission();

    const auto &hwInfo = this->peekHwInfo();
    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::BeforeWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(),
                                                                  DebugPauseState::waitingForUserStartConfirmation,
                                                                  DebugPauseState::hasUserStartConfirmation, hwInfo);
    }

    programEnginePrologue(commandStream);

    if (pageTableManager.get() && !pageTableManagerInitialized) {
        pageTableManagerInitialized = pageTableManager->initPageTableManagerRegisters(this);
    }

    if (logicalStateHelper) {
        logicalStateHelper->writeStreamInline(commandStream, false);
    }

    for (auto &blitProperties : blitPropertiesContainer) {
        TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);
        TimestampPacketHelper::programCsrDependenciesForForTaskCountContainer<GfxFamily>(commandStream, blitProperties.csrDependencies);

        BlitCommandsHelper<GfxFamily>::encodeWa(commandStream, blitProperties, latestSentBcsWaValue);

        if (blitProperties.outputTimestampPacket && profilingEnabled) {
            BlitCommandsHelper<GfxFamily>::encodeProfilingStartMmios(commandStream, *blitProperties.outputTimestampPacket);
        }

        BlitCommandsHelper<GfxFamily>::dispatchBlitCommands(blitProperties, commandStream, *this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex]);

        if (blitProperties.outputTimestampPacket) {
            if (profilingEnabled) {
                MiFlushArgs args;
                EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, 0llu, newTaskCount, args, hwInfo);

                BlitCommandsHelper<GfxFamily>::encodeProfilingEndMmios(commandStream, *blitProperties.outputTimestampPacket);
            } else {
                auto timestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(*blitProperties.outputTimestampPacket);
                MiFlushArgs args;
                args.commandWithPostSync = true;
                EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, timestampPacketGpuAddress, 0, args, hwInfo);
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
    }

    BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(commandStream);

    auto updateTag = !isUpdateTagFromWaitEnabled();
    updateTag |= blocking;
    if (updateTag) {
        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), false, peekHwInfo());

        MiFlushArgs args;
        args.commandWithPostSync = true;
        args.notifyEnable = isUsedNotifyEnableForPostSync();
        EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, tagAllocation->getGpuAddress(), newTaskCount, args, hwInfo);

        MemorySynchronizationCommands<GfxFamily>::addAdditionalSynchronization(commandStream, tagAllocation->getGpuAddress(), false, peekHwInfo());
    }

    if (PauseOnGpuProperties::pauseModeAllowed(DebugManager.flags.PauseOnBlitCopy.get(), taskCount, PauseOnGpuProperties::PauseMode::AfterWorkload)) {
        BlitCommandsHelper<GfxFamily>::dispatchDebugPauseCommands(commandStream, getDebugPauseStateGPUAddress(),
                                                                  DebugPauseState::waitingForUserEndConfirmation,
                                                                  DebugPauseState::hasUserEndConfirmation, hwInfo);
    }

    void *endingCmdPtr = nullptr;
    programEndingCmd(commandStream, device, &endingCmdPtr, blitterDirectSubmission);

    EncodeNoop<GfxFamily>::alignToCacheLine(commandStream);

    makeResident(*tagAllocation);
    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    BatchBuffer batchBuffer{commandStream.getGraphicsAllocation(), commandStreamStart, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStream.getUsed(), &commandStream, endingCmdPtr, false};

    commandStream.getGraphicsAllocation()->updateTaskCount(newTaskCount, this->osContext->getContextId());
    commandStream.getGraphicsAllocation()->updateResidencyTaskCount(newTaskCount, this->osContext->getContextId());

    flush(batchBuffer, getResidencyAllocations());
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
            return std::nullopt;
        }
    }

    return newTaskCount;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::flushTagUpdate() {
    if (this->osContext != nullptr) {
        if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
            this->flushMiFlushDW();
        } else {
            this->flushPipeControl();
        }
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::flushMiFlushDW() {
    auto lock = obtainUniqueOwnership();

    auto &commandStream = getCS(EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite());
    auto commandStreamStart = commandStream.getUsed();

    const auto &hwInfo = this->peekHwInfo();
    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, tagAllocation->getGpuAddress(), taskCount + 1, args, hwInfo);

    makeResident(*tagAllocation);

    this->flushSmallTask(commandStream, commandStreamStart);
    this->latestFlushedTaskCount = taskCount.load();
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushPipeControl() {
    auto lock = obtainUniqueOwnership();

    const auto &hwInfo = peekHwInfo();
    auto &commandStream = getCS(MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo));
    auto commandStreamStart = commandStream.getUsed();

    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    args.workloadPartitionOffset = isMultiTileOperationEnabled();
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(commandStream,
                                                                              PostSyncMode::ImmediateData,
                                                                              getTagAllocation()->getGpuAddress(),
                                                                              taskCount + 1,
                                                                              hwInfo,
                                                                              args);

    makeResident(*tagAllocation);

    this->flushSmallTask(commandStream, commandStreamStart);
    this->latestFlushedTaskCount = taskCount.load();
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::flushSmallTask(LinearStream &commandStreamTask, size_t commandStreamStartTask) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    void *endingCmdPtr = nullptr;

    if (isAnyDirectSubmissionEnabled()) {
        endingCmdPtr = commandStreamTask.getSpace(0);
        EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&commandStreamTask,
                                                                        0ull,
                                                                        false);
    } else {
        auto batchBufferEnd = reinterpret_cast<MI_BATCH_BUFFER_END *>(commandStreamTask.getSpace(sizeof(MI_BATCH_BUFFER_END)));
        *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;
    }

    auto bytesToPad = sizeof(MI_BATCH_BUFFER_START) - sizeof(MI_BATCH_BUFFER_END);
    EncodeNoop<GfxFamily>::emitNoop(commandStreamTask, bytesToPad);
    EncodeNoop<GfxFamily>::alignToCacheLine(commandStreamTask);

    if (globalFenceAllocation) {
        makeResident(*globalFenceAllocation);
    }

    BatchBuffer batchBuffer{commandStreamTask.getGraphicsAllocation(), commandStreamStartTask, 0, nullptr, false, false, QueueThrottle::MEDIUM, QueueSliceCount::defaultSliceCount,
                            commandStreamTask.getUsed(), &commandStreamTask, endingCmdPtr, false};

    this->latestSentTaskCount = taskCount + 1;
    flushHandler(batchBuffer, getResidencyAllocations());
    taskCount++;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::flushHandler(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    flush(batchBuffer, allocationsForResidency);
    makeSurfacePackNonResident(allocationsForResidency, true);
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isUpdateTagFromWaitEnabled() {
    auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
    auto enabled = hwHelper.isUpdateTaskCountFromWaitSupported();
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
inline void CommandStreamReceiverHw<GfxFamily>::programAdditionalStateBaseAddress(LinearStream &csr, typename GfxFamily::STATE_BASE_ADDRESS &cmd, Device &device) {}

template <typename GfxFamily>
inline MemoryCompressionState CommandStreamReceiverHw<GfxFamily>::getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const {
    return MemoryCompressionState::NotApplicable;
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isPipelineSelectAlreadyProgrammed() const {
    const auto &hwInfoConfig = *HwInfoConfig::get(peekHwInfo().platform.eProductFamily);
    return this->streamProperties.stateComputeMode.isDirty() && hwInfoConfig.is3DPipelineSelectWARequired() && isRcs();
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programEpilogue(LinearStream &csr, Device &device, void **batchBufferEndLocation, DispatchFlags &dispatchFlags) {
    if (dispatchFlags.epilogueRequired) {
        auto currentOffset = ptrDiff(csr.getSpace(0u), csr.getCpuBase());
        auto gpuAddress = ptrOffset(csr.getGraphicsAllocation()->getGpuAddress(), currentOffset);

        addBatchBufferStart(reinterpret_cast<typename GfxFamily::MI_BATCH_BUFFER_START *>(*batchBufferEndLocation), gpuAddress, false);
        this->programEpliogueCommands(csr, dispatchFlags);
        programEndingCmd(csr, device, batchBufferEndLocation, isDirectSubmissionEnabled());
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
inline void CommandStreamReceiverHw<GfxFamily>::stopDirectSubmission() {
    if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
        this->blitterDirectSubmission->stopRingBuffer();
    } else {
        this->directSubmission->stopRingBuffer();
    }
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::initDirectSubmission() {
    bool ret = true;

    bool submitOnInit = false;
    auto startDirect = this->osContext->isDirectSubmissionAvailable(peekHwInfo(), submitOnInit);

    if (startDirect) {
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
        this->osContext->setDirectSubmissionActive();
    }
    return ret;
}

template <typename GfxFamily>
TagAllocatorBase *CommandStreamReceiverHw<GfxFamily>::getTimestampPacketAllocator() {
    if (timestampPacketAllocator.get() == nullptr) {
        auto &hwHelper = HwHelper::get(peekHwInfo().platform.eRenderCoreFamily);
        const RootDeviceIndicesContainer rootDeviceIndices = {rootDeviceIndex};

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
    return EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(this->peekHwInfo(), hasSharedHandles(), isRcs());
}

template <typename GfxFamily>
constexpr bool CommandStreamReceiverHw<GfxFamily>::isGlobalAtomicsProgrammingRequired(bool currentVal) const {
    return false;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::createKernelArgsBufferAllocation() {
}

} // namespace NEO
