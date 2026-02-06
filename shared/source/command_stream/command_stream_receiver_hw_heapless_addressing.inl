/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/bindless_heaps_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleAllocationsResidencyForflushTaskStateless(
    const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh, Device &device) {

    auto &hwInfo = this->peekHwInfo();
    const bool hasDsh = hwInfo.capabilityTable.supportsImages && dsh != nullptr;

    makeResident(*tagAllocation);

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    if (getPreemptionAllocation()) {
        makeResident(*getPreemptionAllocation());
    }

    if (hasDsh) {
        auto dshAllocation = dsh->getGraphicsAllocation();
        this->makeResident(*dshAllocation);
        dshAllocation->setEvictable(false);
    }

    if (ssh) {
        auto sshAllocation = ssh->getGraphicsAllocation();
        this->makeResident(*sshAllocation);
    }

    if (ioh) {
        auto iohAllocation = ioh->getGraphicsAllocation();
        this->makeResident(*iohAllocation);
        iohAllocation->setEvictable(false);
    }

    if (device.isStateSipRequired()) {
        makeResident(*SipKernel::getSipKernel(device, osContext).getSipAllocation());
    }

    bool debuggingEnabled = device.getDebugger() && debugSurface;
    if (debuggingEnabled) {
        makeResident(*debugSurface);
    }

    if (getWorkPartitionAllocation()) {
        makeResident(*getWorkPartitionAllocation());
    }

    auto rtBuffer = device.getRTMemoryBackedBuffer();
    if (rtBuffer) {
        makeResident(*rtBuffer);
    }
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTaskHeapless(
    LinearStream &commandStream, size_t commandStreamStart,
    const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
    TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) {

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "taskLevel", taskLevel);

    bool levelClosed = false;
    bool hasStallingCmdsOnTaskStream = false;

    bool implicitFlush = dispatchFlags.implicitFlush ||
                         dispatchFlags.blocking ||
                         debugManager.flags.ForceImplicitFlush.get();

    bool barrierWithPostSyncNeeded = dispatchFlags.blocking ||
                                     dispatchFlags.dcFlush ||
                                     dispatchFlags.guardCommandBufferWithPipeControl ||
                                     dispatchFlags.textureCacheFlush ||
                                     isRecyclingTagForHeapStorageRequired();

    PipeControlArgs args;
    void *currentPipeControlForNooping = nullptr;
    void *epiloguePipeControlLocation = nullptr;
    this->isWalkerWithProfilingEnqueued |= dispatchFlags.isWalkerWithProfilingEnqueued;

    if (!heaplessStateInitialized) {
        bool isHeaplessStateInit = EngineHelpers::isCcs(this->osContext->getEngineType());
        if (isHeaplessStateInit) {
            initializeDeviceWithFirstSubmission(device);
        }
        heaplessStateInitialized = true;
    }
    if (this->ucResourceRequiresTagUpdate) {
        this->emitTagUpdateWithoutDCFlush(commandStream);
    }

    if (debugManager.flags.ForceCsrFlushing.get()) {
        flushBatchedSubmissions();
    }

    if (barrierWithPostSyncNeeded) {
        processBarrierWithPostSync(commandStream, dispatchFlags, levelClosed, currentPipeControlForNooping,
                                   epiloguePipeControlLocation, hasStallingCmdsOnTaskStream, args);
    }

    this->latestSentTaskCount = taskCount + 1;

    auto estimatedSize = getRequiredCmdStreamHeaplessSizeAligned(dispatchFlags, device);
    auto &commandStreamCSR = this->getCS(estimatedSize);
    auto commandStreamStartCSR = commandStreamCSR.getUsed();

    if (isProgramActivePartitionConfigRequired()) {
        programActivePartitionConfig(commandStreamCSR);
    }

    const bool useSemaphore64bCmd = device.getDeviceInfo().semaphore64bCmdSupport;
    TimestampPacketHelper::programCsrDependenciesForTimestampPacketContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies, false, EngineHelpers::isBcs(this->osContext->getEngineType()), useSemaphore64bCmd);
    TimestampPacketHelper::programCsrDependenciesForForMultiRootDeviceSyncContainer<GfxFamily>(commandStreamCSR, dispatchFlags.csrDependencies, useSemaphore64bCmd);

    if (dispatchFlags.isStallingCommandsOnNextFlushRequired) {
        programStallingCommandsForBarrier(commandStreamCSR, dispatchFlags.barrierTimestampPacketNodes, dispatchFlags.isDcFlushRequiredOnStallingCommandsOnNextFlush);
    }

    DBG_LOG(LogTaskCounts, __FUNCTION__, "Line: ", __LINE__, "this->taskLevel", (uint32_t)this->taskLevel);

    addPipeControlFlushTaskIfNeeded(commandStreamCSR, taskLevel);

    handleAllocationsResidencyForflushTaskStateless(dsh, ioh, ssh, device);

    bool submitTask = commandStreamStart != commandStream.getUsed();
    bool submitCSR = commandStreamStartCSR != commandStreamCSR.getUsed();

    auto batchBuffer = prepareBatchBufferForSubmission(commandStream, commandStreamStart, commandStreamCSR, commandStreamStartCSR,
                                                       dispatchFlags, device, submitTask, submitCSR, hasStallingCmdsOnTaskStream);

    auto completionStamp = handleFlushTaskSubmission(std::move(batchBuffer), dispatchFlags, device, currentPipeControlForNooping, epiloguePipeControlLocation,
                                                     args, submitTask, submitCSR, hasStallingCmdsOnTaskStream, levelClosed, implicitFlush);

    return completionStamp;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamHeaplessSize(const DispatchFlags &dispatchFlags, Device &device) {
    size_t size = 0u;
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    size += sizeof(typename GfxFamily::MI_BATCH_BUFFER_START);
    size += getCmdSizeForEpilogue(dispatchFlags);

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

    if (this->isProgramActivePartitionConfigRequired()) {
        size += this->getCmdSizeForActivePartitionConfig();
    }

    return size;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamHeaplessSizeAligned(const DispatchFlags &dispatchFlags, Device &device) {

    auto size = getRequiredCmdStreamHeaplessSize(dispatchFlags, device);
    return alignUp(size, MemoryConstants::cacheLineSize);
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushImmediateTaskStateless(
    LinearStream &immediateCommandStream,
    size_t immediateCommandStreamStart,
    ImmediateDispatchFlags &dispatchFlags,
    Device &device) {

    this->isWalkerWithProfilingEnqueued |= dispatchFlags.isWalkerWithProfilingEnqueued;
    ImmediateFlushData flushData;

    if (dispatchFlags.dispatchOperation != AppendOperations::cmdList) {
        if (!heaplessStateInitialized) {
            bool isHeaplessStateInit = EngineHelpers::isCcs(this->osContext->getEngineType());
            if (isHeaplessStateInit) {
                initializeDeviceWithFirstSubmission(device);
            }
            heaplessStateInitialized = true;
        }

        flushData.stateCacheFlushRequired = device.getBindlessHeapsHelper() ? device.getBindlessHeapsHelper()->getStateDirtyForContext(getOsContext().getContextId()) : false;
        if (flushData.stateCacheFlushRequired) {
            flushData.estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        }

        if (this->requiresInstructionCacheFlush) {
            flushData.estimatedSize += MemorySynchronizationCommands<GfxFamily>::getSizeForInstructionCacheFlush();
        }

        if (this->isProgramActivePartitionConfigRequired()) {
            flushData.estimatedSize += this->getCmdSizeForActivePartitionConfig();
        }

        if (this->isPerQueuePrologueEnabled()) {
            flushData.estimatedSize += getCmdSizeForPrologue();
        }
    }

    // this must be the last call after all estimate size operations
    handleImmediateFlushJumpToImmediate(flushData);

    auto &csrCommandStream = getCS(flushData.estimatedSize);
    flushData.csrStartOffset = csrCommandStream.getUsed();

    if (dispatchFlags.dispatchOperation != AppendOperations::cmdList) {
        if (isPerQueuePrologueEnabled()) {
            programEnginePrologue(csrCommandStream);
        }

        if (isProgramActivePartitionConfigRequired()) {
            programActivePartitionConfig(csrCommandStream);
        }

        if (flushData.stateCacheFlushRequired) {
            device.getBindlessHeapsHelper()->clearStateDirtyForContext(getOsContext().getContextId());
            MemorySynchronizationCommands<GfxFamily>::addStateCacheFlush(csrCommandStream, device.getRootDeviceEnvironment());
        }

        if (this->requiresInstructionCacheFlush) {
            MemorySynchronizationCommands<GfxFamily>::addInstructionCacheFlush(csrCommandStream);
            this->requiresInstructionCacheFlush = false;
        }
    }

    dispatchImmediateFlushJumpToImmediateCommand(immediateCommandStream, immediateCommandStreamStart, flushData, csrCommandStream);
    dispatchImmediateFlushClientBufferCommands(dispatchFlags, immediateCommandStream, flushData);

    handleImmediateFlushStatelessAllocationsResidency(flushData.estimatedSize,
                                                      csrCommandStream,
                                                      device);

    return handleImmediateFlushSendBatchBuffer(immediateCommandStream,
                                               immediateCommandStreamStart,
                                               dispatchFlags,
                                               flushData,
                                               csrCommandStream);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleImmediateFlushStatelessAllocationsResidency(size_t csrEstimatedSize,
                                                                                           LinearStream &csrStream,
                                                                                           Device &device) {
    this->makeResident(*this->tagAllocation);

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    if (getWorkPartitionAllocation()) {
        makeResident(*getWorkPartitionAllocation());
    }

    if (device.getRTMemoryBackedBuffer()) {
        makeResident(*device.getRTMemoryBackedBuffer());
    }

    if (getPreemptionAllocation()) {
        makeResident(*getPreemptionAllocation());
    }

    if (csrEstimatedSize) {
        makeResident(*csrStream.getGraphicsAllocation());
    }

    if (device.isStateSipRequired()) {
        makeResident(*SipKernel::getSipKernel(device, osContext).getSipAllocation());
    }
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::programHeaplessProlog(Device &device) {

    auto lock = obtainUniqueOwnership();

    auto &rootDeviceEnvironment = peekRootDeviceEnvironment();
    PipeControlArgs args;
    args.notifyEnable = isUsedNotifyEnableForPostSync();
    args.workloadPartitionOffset = isMultiTileOperationEnabled();
    args.textureCacheInvalidationEnable = true;
    args.renderTargetCacheFlushEnable = true;
    args.stateCacheInvalidationEnable = true;
    args.tlbInvalidation = true;
    args.dcFlushEnable = true;

    auto dispatchSize = MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData) +
                        this->getCmdSizeForHeaplessPrologue(device);

    auto &commandStream = getCS(dispatchSize);
    auto commandStreamStart = commandStream.getUsed();

    programHeaplessStateProlog(device, commandStream);

    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(commandStream,
                                                                              PostSyncMode::immediateData,
                                                                              getTagAllocation()->getGpuAddress(),
                                                                              taskCount + 1,
                                                                              rootDeviceEnvironment,
                                                                              args);

    handleAllocationsResidencyForHeaplessProlog(commandStream, device);

    auto submissionStatus = this->flushSmallTask(commandStream, commandStreamStart);
    this->latestFlushedTaskCount = taskCount.load();

    return submissionStatus;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForHeaplessPrologue(Device &device) const {

    using STATE_CONTEXT_DATA_BASE_ADDRESS = typename GfxFamily::STATE_CONTEXT_DATA_BASE_ADDRESS;
    using _3DSTATE_BTD = typename GfxFamily::_3DSTATE_BTD;

    size_t size = 0u;
    size += getCmdSizeForPrologue();
    size += StateBaseAddressHelper<GfxFamily>::getSbaCmdSize();
    size += EncodeComputeMode<GfxFamily>::getSizeForComputeMode();

    bool debuggingEnabled = device.getDebugger() != nullptr;
    bool isMidThreadPreemption = device.getPreemptionMode() == PreemptionMode::MidThread;
    if (debuggingEnabled || isMidThreadPreemption) {
        size += sizeof(STATE_CONTEXT_DATA_BASE_ADDRESS);
    }
    if (debuggingEnabled) {
        size += getCmdSizeForExceptions();
    }

    size += PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(device, false);
    size += getCmdSizeForActivePartitionConfig();

    auto rayTracingBuffer = device.getRTMemoryBackedBuffer();
    if (rayTracingBuffer) {
        size += sizeof(_3DSTATE_BTD);
    }
    return size;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programHeaplessStateProlog(Device &device, LinearStream &commandStream) {

    bool perQueueStateOnly = this->executionEnvironment.rootDeviceEnvironments[getRootDeviceIndex()]->osInterface &&
                             this->executionEnvironment.rootDeviceEnvironments[getRootDeviceIndex()]->osInterface->getAggregatedProcessCount() > 1;

    perQueueStateOnly &= (this->osContext->getPrimaryContext() != nullptr);

    if (!perQueueStateOnly) {
        programComputeModeHeapless(device, commandStream);
    }
    programEnginePrologue(commandStream);
    programStateBaseAddressHeapless(device, commandStream);

    if (!perQueueStateOnly) {
        if (isProgramActivePartitionConfigRequired()) {
            programActivePartitionConfig(commandStream);
        }
    }

    if (getReleaseHelper()->isRayTracingSupported()) {
        device.initializeRTMemoryBackedBuffer();
    }

    if (device.getRTMemoryBackedBuffer()) {
        this->dispatchRayTracingStateCommand(commandStream, device);
    }

    bool isDebuggerActive = device.getDebugger() != nullptr;
    if (isDebuggerActive) {
        PreemptionHelper::programCsrBaseAddress<GfxFamily>(commandStream, device, device.getDebugSurface());
        this->programExceptions(commandStream, device);
    } else {
        PreemptionHelper::programCsrBaseAddress<GfxFamily>(commandStream, device, getPreemptionAllocation());
    }

    setPreemptionMode(device.getPreemptionMode());
    if (!perQueueStateOnly) {
        programStateSip(commandStream, device);
    }
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::initializeDeviceWithFirstSubmission(Device &device) {
    bool isSecondaryContext = this->osContext->getPrimaryContext() != nullptr;
    SubmissionStatus status = SubmissionStatus::success;

    bool aggregatedProcesses = this->executionEnvironment.rootDeviceEnvironments[getRootDeviceIndex()]->osInterface &&
                               this->executionEnvironment.rootDeviceEnvironments[getRootDeviceIndex()]->osInterface->getAggregatedProcessCount() > 1;

    bool skipInitialization = !aggregatedProcesses && isSecondaryContext;

    if (skipInitialization || this->latestFlushedTaskCount > 0) {
        return status;
    }

    this->initDirectSubmission();
    if (EngineHelpers::isBcs(this->osContext->getEngineType())) {
        status = this->flushMiFlushDW(true);
    } else {

        if (ApiSpecificConfig::isGlobalStatelessEnabled(device.getRootDeviceEnvironment())) {
            this->createGlobalStatelessHeap();
        }
        status = programHeaplessProlog(device);
    }

    if (isTbxMode() && (status == SubmissionStatus::success)) {
        waitForCompletionWithTimeout({true, false, true, TimeoutControls::maxTimeout}, this->taskCount);
    }

    heaplessStateInitialized = true;
    return status;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeModeHeapless(Device &device, LinearStream &commandStream) {

    this->streamProperties.stateComputeMode.setPropertiesAll(false, 0u, 0u, device.getPreemptionMode(), device.hasAnyPeerAccess());

    const RootDeviceEnvironment &rootDeviceEnvironment = device.getRootDeviceEnvironment();
    EncodeComputeMode<Family>::programComputeModeCommand(commandStream, this->streamProperties.stateComputeMode, rootDeviceEnvironment);

    this->setStateComputeModeDirty(false);
    this->streamProperties.stateComputeMode.clearIsDirty();
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleAllocationsResidencyForHeaplessProlog(LinearStream &commandStream, Device &device) {

    makeResident(*this->tagAllocation);
    makeResident(*commandStream.getGraphicsAllocation());

    if (getGlobalFenceAllocation()) {
        makeResident(*getGlobalFenceAllocation());
    }

    if (this->getPreemptionAllocation()) {
        makeResident(*this->getPreemptionAllocation());
    }

    if (device.isStateSipRequired()) {
        GraphicsAllocation *sipAllocation = SipKernel::getSipKernel(device, this->osContext).getSipAllocation();
        makeResident(*sipAllocation);
    }

    if (device.getDebugger() && this->debugSurface) {
        makeResident(*this->debugSurface);
    }

    if (getWorkPartitionAllocation()) {
        makeResident(*getWorkPartitionAllocation());
    }

    auto rayTracingBuffer = device.getRTMemoryBackedBuffer();
    if (rayTracingBuffer) {
        makeResident(*rayTracingBuffer);
    }
}

} // namespace NEO
