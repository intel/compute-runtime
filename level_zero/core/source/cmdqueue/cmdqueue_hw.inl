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
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/command_stream/thread_arbitration_policy.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/memory_manager.h"
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
    if (commandStream) {
        delete commandStream;
        commandStream = nullptr;
    }
    buffers.destroy(this->getDevice());
    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
        device->getL0Debugger()->notifyCommandQueueDestroyed();
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

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    using MI_LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    auto lockCSR = csr->obtainUniqueOwnership();

    auto anyCommandListWithCooperativeKernels = false;
    auto anyCommandListWithoutCooperativeKernels = false;
    bool anyCommandListRequiresDisabledEUFusion = false;
    bool cachedMOCSAllowed = true;

    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        if (peekIsCopyOnlyCommandQueue() != commandList->isCopyOnly()) {
            return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
        }

        if (this->activeSubDevices < commandList->partitionCount) {
            return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
        }

        if (commandList->containsCooperativeKernels()) {
            anyCommandListWithCooperativeKernels = true;
        } else {
            anyCommandListWithoutCooperativeKernels = true;
        }

        if (commandList->getRequiredStreamState().frontEndState.disableEUFusion.value == 1) {
            anyCommandListRequiresDisabledEUFusion = true;
        }

        // If the Command List has commands that require uncached MOCS, then any changes to the commands in the queue requires the uncached MOCS
        if (commandList->requiresQueueUncachedMocs && cachedMOCSAllowed == true) {
            cachedMOCSAllowed = false;
        }
    }

    bool isMixingRegularAndCooperativeKernelsAllowed = NEO::DebugManager.flags.AllowMixingRegularAndCooperativeKernels.get();
    if (anyCommandListWithCooperativeKernels && anyCommandListWithoutCooperativeKernels &&
        (!isMixingRegularAndCooperativeKernelsAllowed)) {
        return ZE_RESULT_ERROR_INVALID_COMMAND_LIST_TYPE;
    }

    size_t spaceForResidency = 0;
    size_t preemptionSize = 0u;
    size_t debuggerCmdsSize = 0;
    constexpr size_t residencyContainerSpaceForPreemption = 2;
    constexpr size_t residencyContainerSpaceForTagWrite = 1;

    NEO::Device *neoDevice = device->getNEODevice();
    auto devicePreemption = device->getDevicePreemptionMode();
    const bool initialPreemptionMode = commandQueuePreemptionMode == NEO::PreemptionMode::Initial;
    NEO::PreemptionMode cmdQueuePreemption = commandQueuePreemptionMode;
    if (initialPreemptionMode) {
        cmdQueuePreemption = devicePreemption;
    }
    NEO::PreemptionMode statePreemption = cmdQueuePreemption;

    const bool stateSipRequired = (initialPreemptionMode && devicePreemption == NEO::PreemptionMode::MidThread) ||
                                  (neoDevice->getDebugger() && NEO::Debugger::isDebugEnabled(internalUsage));

    if (initialPreemptionMode) {
        preemptionSize += NEO::PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*neoDevice);
    }

    if (stateSipRequired) {
        preemptionSize += NEO::PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*neoDevice, csr->isRcs());
    }

    preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(devicePreemption, commandQueuePreemptionMode);

    if (NEO::Debugger::isDebugEnabled(internalUsage) && !commandQueueDebugCmdsProgrammed) {
        debuggerCmdsSize += NEO::PreambleHelper<GfxFamily>::getKernelDebuggingCommandsSize(neoDevice->getSourceLevelDebugger() != nullptr);
    }

    if (devicePreemption == NEO::PreemptionMode::MidThread) {
        spaceForResidency += residencyContainerSpaceForPreemption;
    }

    bool directSubmissionEnabled = isCopyOnlyCommandQueue ? csr->isBlitterDirectSubmissionEnabled() : csr->isDirectSubmissionEnabled();
    bool programActivePartitionConfig = csr->isProgramActivePartitionConfigRequired();

    L0::Fence *fence = nullptr;

    device->activateMetricGroups();

    size_t totalCmdBuffers = 0;
    uint32_t perThreadScratchSpaceSize = 0;
    uint32_t perThreadPrivateScratchSize = 0;
    NEO::PageFaultManager *pageFaultManager = nullptr;
    if (performMigration) {
        pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
        if (pageFaultManager == nullptr) {
            performMigration = false;
        }
    }
    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);

        commandList->csr = csr;
        commandList->handleIndirectAllocationResidency();

        totalCmdBuffers += commandList->commandContainer.getCmdBufferAllocations().size();
        spaceForResidency += commandList->commandContainer.getResidencyContainer().size();
        auto commandListPreemption = commandList->getCommandListPreemptionMode();
        if (statePreemption != commandListPreemption) {
            if (preemptionCmdSyncProgramming) {
                preemptionSize += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl();
            }
            preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(commandListPreemption, statePreemption);
            statePreemption = commandListPreemption;
        }

        perThreadScratchSpaceSize = std::max(perThreadScratchSpaceSize, commandList->getCommandListPerThreadScratchSize());

        perThreadPrivateScratchSize = std::max(perThreadPrivateScratchSize, commandList->getCommandListPerThreadPrivateScratchSize());

        if (commandList->getCommandListPerThreadScratchSize() != 0 || commandList->getCommandListPerThreadPrivateScratchSize() != 0) {
            if (commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE) != nullptr) {
                heapContainer.push_back(commandList->commandContainer.getIndirectHeap(NEO::HeapType::SURFACE_STATE)->getGraphicsAllocation());
            }
            for (auto element : commandList->commandContainer.sshAllocations) {
                heapContainer.push_back(element);
            }
        }

        partitionCount = std::max(partitionCount, commandList->partitionCount);
        commandList->makeResidentAndMigrate(performMigration);
    }

    size_t linearStreamSizeEstimate = totalCmdBuffers * sizeof(MI_BATCH_BUFFER_START);
    linearStreamSizeEstimate += csr->getCmdsSizeForHardwareContext();
    if (directSubmissionEnabled) {
        linearStreamSizeEstimate += sizeof(MI_BATCH_BUFFER_START);
    } else {
        linearStreamSizeEstimate += sizeof(MI_BATCH_BUFFER_END);
    }

    auto csrHw = reinterpret_cast<NEO::CommandStreamReceiverHw<GfxFamily> *>(csr);
    if (programActivePartitionConfig) {
        linearStreamSizeEstimate += csrHw->getCmdSizeForActivePartitionConfig();
    }
    const auto &hwInfo = this->device->getHwInfo();
    if (hFence) {
        linearStreamSizeEstimate += isCopyOnlyCommandQueue ? NEO::EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite() : NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo);
    }
    spaceForResidency += residencyContainerSpaceForTagWrite;

    csr->getResidencyAllocations().reserve(spaceForResidency);

    auto scratchSpaceController = csr->getScratchSpaceController();
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    handleScratchSpace(heapContainer,
                       scratchSpaceController,
                       gsbaStateDirty, frontEndStateDirty,
                       perThreadScratchSpaceSize, perThreadPrivateScratchSize);

    auto &streamProperties = csr->getStreamProperties();
    const auto &hwInfoConfig = *NEO::HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto disableOverdispatch = hwInfoConfig.isDisableOverdispatchAvailable(hwInfo);
    auto isEngineInstanced = csr->getOsContext().isEngineInstanced();
    bool isPatchingVfeStateAllowed = NEO::DebugManager.flags.AllowPatchingVfeStateInCommandLists.get();
    if (!isPatchingVfeStateAllowed) {
        streamProperties.frontEndState.setProperties(anyCommandListWithCooperativeKernels, anyCommandListRequiresDisabledEUFusion,
                                                     disableOverdispatch, isEngineInstanced, hwInfo);
    } else {
        streamProperties.frontEndState.singleSliceDispatchCcsMode.set(isEngineInstanced);
    }
    frontEndStateDirty |= streamProperties.frontEndState.isDirty();

    gsbaStateDirty |= csr->getGSBAStateDirty();
    frontEndStateDirty |= csr->getMediaVFEStateDirty();
    if (!isCopyOnlyCommandQueue) {

        if (!gpgpuEnabled) {
            linearStreamSizeEstimate += estimatePipelineSelect();
        }

        linearStreamSizeEstimate += estimateFrontEndCmdSizeForMultipleCommandLists(frontEndStateDirty, numCommandLists, phCommandLists);
        linearStreamSizeEstimate += estimateStateComputeModeCmdSizeForMultipleCommandLists(numCommandLists, phCommandLists);

        if (gsbaStateDirty) {
            linearStreamSizeEstimate += estimateStateBaseAddressCmdSize();
        }

        linearStreamSizeEstimate += preemptionSize + debuggerCmdsSize;
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        linearStreamSizeEstimate += NEO::SWTagsManager::estimateSpaceForSWTags<GfxFamily>();
    }

    linearStreamSizeEstimate += isCopyOnlyCommandQueue ? NEO::EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite() : NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo);

    size_t alignedSize = alignUp<size_t>(linearStreamSizeEstimate, minCmdBufferPtrAlign);
    size_t padding = alignedSize - linearStreamSizeEstimate;
    reserveLinearStreamSize(alignedSize);
    NEO::LinearStream child(commandStream->getSpace(alignedSize), alignedSize);

    const auto globalFenceAllocation = csr->getGlobalFenceAllocation();
    if (globalFenceAllocation) {
        csr->makeResident(*globalFenceAllocation);
    }

    const auto workPartitionAllocation = csr->getWorkPartitionAllocation();
    if (workPartitionAllocation) {
        csr->makeResident(*workPartitionAllocation);
    }

    if (NEO::DebugManager.flags.EnableSWTags.get()) {
        NEO::SWTagsManager *tagsManager = neoDevice->getRootDeviceEnvironment().tagsManager.get();
        UNRECOVERABLE_IF(tagsManager == nullptr);
        csr->makeResident(*tagsManager->getBXMLHeapAllocation());
        csr->makeResident(*tagsManager->getSWTagHeapAllocation());
        tagsManager->insertBXMLHeapAddress<GfxFamily>(child);
        tagsManager->insertSWTagHeapAddress<GfxFamily>(child);
    }

    csr->programHardwareContext(child);

    if (NEO::Debugger::isDebugEnabled(internalUsage) && device->getL0Debugger()) {
        csr->makeResident(*device->getL0Debugger()->getSbaTrackingBuffer(csr->getOsContext().getContextId()));
    }

    if (!isCopyOnlyCommandQueue) {
        if (!gpgpuEnabled) {
            programPipelineSelect(child);
        }

        if (NEO::Debugger::isDebugEnabled(internalUsage) && !commandQueueDebugCmdsProgrammed && neoDevice->getSourceLevelDebugger()) {
            NEO::PreambleHelper<GfxFamily>::programKernelDebugging(&child);
            commandQueueDebugCmdsProgrammed = true;
        }

        if (gsbaStateDirty) {
            auto indirectHeap = CommandList::fromHandle(phCommandLists[0])->commandContainer.getIndirectHeap(NEO::HeapType::INDIRECT_OBJECT);
            programStateBaseAddress(scratchSpaceController->calculateNewGSH(), indirectHeap->getGraphicsAllocation()->isAllocatedInLocalMemoryPool(), child, cachedMOCSAllowed);
        }

        if (initialPreemptionMode) {
            NEO::PreemptionHelper::programCsrBaseAddress<GfxFamily>(child, *neoDevice, csr->getPreemptionAllocation());
        }

        if (stateSipRequired) {
            NEO::PreemptionHelper::programStateSip<GfxFamily>(child, *neoDevice);
        }

        if (cmdQueuePreemption != commandQueuePreemptionMode) {
            NEO::PreemptionHelper::programCmdStream<GfxFamily>(child,
                                                               cmdQueuePreemption,
                                                               commandQueuePreemptionMode,
                                                               csr->getPreemptionAllocation());
        }

        statePreemption = cmdQueuePreemption;

        const bool sipKernelUsed = devicePreemption == NEO::PreemptionMode::MidThread ||
                                   (neoDevice->getDebugger() != nullptr && NEO::Debugger::isDebugEnabled(internalUsage));

        if (devicePreemption == NEO::PreemptionMode::MidThread) {
            csr->makeResident(*csr->getPreemptionAllocation());
        }

        if (sipKernelUsed) {
            auto sipIsa = NEO::SipKernel::getSipKernel(*neoDevice).getSipAllocation();
            csr->makeResident(*sipIsa);
        }

        if (NEO::Debugger::isDebugEnabled(internalUsage) && neoDevice->getDebugger()) {
            UNRECOVERABLE_IF(device->getDebugSurface() == nullptr);
            csr->makeResident(*device->getDebugSurface());
        }
    }

    if (programActivePartitionConfig) {
        csrHw->programActivePartitionConfig(child);
    }

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto cmdBufferAllocations = commandList->commandContainer.getCmdBufferAllocations();
        auto cmdBufferCount = cmdBufferAllocations.size();

        auto commandListPreemption = commandList->getCommandListPreemptionMode();
        if (statePreemption != commandListPreemption) {
            if (NEO::DebugManager.flags.EnableSWTags.get()) {
                neoDevice->getRootDeviceEnvironment().tagsManager->insertTag<GfxFamily, NEO::SWTags::PipeControlReasonTag>(
                    child,
                    *neoDevice,
                    "ComandList Preemption Mode update", 0u);
            }

            if (preemptionCmdSyncProgramming) {
                NEO::PipeControlArgs args;
                NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(child, args);
            }
            NEO::PreemptionHelper::programCmdStream<GfxFamily>(child,
                                                               commandListPreemption,
                                                               statePreemption,
                                                               csr->getPreemptionAllocation());
            statePreemption = commandListPreemption;
        }

        if (!isCopyOnlyCommandQueue) {
            auto &requiredStreamState = commandList->getRequiredStreamState();
            streamProperties.stateComputeMode.setProperties(requiredStreamState.stateComputeMode);

            bool programVfe = frontEndStateDirty;
            if (isPatchingVfeStateAllowed) {
                streamProperties.frontEndState.setProperties(requiredStreamState.frontEndState);
                programVfe |= streamProperties.frontEndState.isDirty();
            }

            if (programVfe) {
                programFrontEnd(scratchSpaceController->getScratchPatchAddress(), scratchSpaceController->getPerThreadScratchSpaceSize(), child);
                frontEndStateDirty = false;
            }
            if (streamProperties.stateComputeMode.isDirty()) {
                NEO::EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(
                    child, streamProperties.stateComputeMode, {}, false, hwInfo, csr->isRcs());
            }

            auto &finalStreamState = commandList->getFinalStreamState();
            streamProperties.stateComputeMode.setProperties(finalStreamState.stateComputeMode);
            if (isPatchingVfeStateAllowed) {
                streamProperties.frontEndState.setProperties(finalStreamState.frontEndState);
            }
        }

        patchCommands(*commandList, scratchSpaceController->getScratchPatchAddress());

        for (size_t iter = 0; iter < cmdBufferCount; iter++) {
            auto allocation = cmdBufferAllocations[iter];
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&child, allocation->getGpuAddress(), true);
        }

        printfFunctionContainer.insert(printfFunctionContainer.end(),
                                       commandList->getPrintfFunctionContainer().begin(),
                                       commandList->getPrintfFunctionContainer().end());
    }

    if (performMigration) {
        auto commandList = CommandList::fromHandle(phCommandLists[0]);
        commandList->migrateSharedAllocations();
    }

    if (stateSipRequired) {
        NEO::PreemptionHelper::programStateSipEndWa<GfxFamily>(child, *neoDevice);
    }

    commandQueuePreemptionMode = statePreemption;

    if (hFence) {
        fence = Fence::fromHandle(hFence);
        fence->assignTaskCountFromCsr();
        if (isCopyOnlyCommandQueue) {
            NEO::MiFlushArgs args;
            args.commandWithPostSync = true;
            NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(child, csr->getTagAllocation()->getGpuAddress(), csr->peekTaskCount() + 1, args, hwInfo);
        } else {
            NEO::PipeControlArgs args;
            args.dcFlushEnable = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
            if (partitionCount > 1) {
                args.workloadPartitionOffset = true;
            }
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
                child, POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
                csr->getTagAllocation()->getGpuAddress(),
                csr->peekTaskCount() + 1,
                hwInfo,
                args);
        }
    }

    dispatchTaskCountWrite(child, true);

    csr->makeResident(*csr->getTagAllocation());
    void *endingCmd = nullptr;
    if (directSubmissionEnabled) {
        auto offset = ptrDiff(child.getCpuBase(), commandStream->getCpuBase()) + child.getUsed();
        uint64_t startAddress = commandStream->getGraphicsAllocation()->getGpuAddress() + offset;
        if (NEO::DebugManager.flags.BatchBufferStartPrepatchingWaEnabled.get() == 0) {
            startAddress = 0;
        }

        endingCmd = child.getSpace(0);
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&child, startAddress, false);
    } else {
        MI_BATCH_BUFFER_END cmd = GfxFamily::cmdInitBatchBufferEnd;
        auto buffer = child.getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *(MI_BATCH_BUFFER_END *)buffer = cmd;
    }

    if (padding) {
        void *paddingPtr = child.getSpace(padding);
        memset(paddingPtr, 0, padding);
    }

    auto ret = submitBatchBuffer(ptrDiff(child.getCpuBase(), commandStream->getCpuBase()), csr->getResidencyAllocations(), endingCmd,
                                 anyCommandListWithCooperativeKernels);

    this->taskCount = csr->peekTaskCount();

    csr->makeSurfacePackNonResident(csr->getResidencyAllocations());

    if (getSynchronousMode() == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS) {
        const auto synchronizeResult = this->synchronize(std::numeric_limits<uint64_t>::max());
        if (synchronizeResult == ZE_RESULT_ERROR_DEVICE_LOST) {
            return ZE_RESULT_ERROR_DEVICE_LOST;
        }
    }

    this->heapContainer.clear();

    csr->pollForCompletion();

    if (ret != NEO::SubmissionStatus::SUCCESS) {
        if (ret == NEO::SubmissionStatus::OUT_OF_MEMORY) {
            return ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programFrontEnd(uint64_t scratchAddress, uint32_t perThreadScratchSpaceSize, NEO::LinearStream &commandStream) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    UNRECOVERABLE_IF(csr == nullptr);
    auto &hwInfo = device->getHwInfo();
    auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto engineGroupType = hwHelper.getEngineGroupType(csr->getOsContext().getEngineType(),
                                                       csr->getOsContext().getEngineUsage(), hwInfo);
    auto pVfeState = NEO::PreambleHelper<GfxFamily>::getSpaceForVfeState(&commandStream, hwInfo, engineGroupType);
    NEO::PreambleHelper<GfxFamily>::programVfeState(pVfeState,
                                                    hwInfo,
                                                    perThreadScratchSpaceSize,
                                                    scratchAddress,
                                                    device->getMaxNumHwThreads(),
                                                    csr->getStreamProperties());
    csr->setMediaVFEStateDirty(false);
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSize() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    return NEO::PreambleHelper<GfxFamily>::getVFECommandsSize();
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSizeForMultipleCommandLists(
    bool isFrontEndStateDirty, uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists) {

    auto singleFrontEndCmdSize = estimateFrontEndCmdSize();
    bool isPatchingVfeStateAllowed = NEO::DebugManager.flags.AllowPatchingVfeStateInCommandLists.get();
    if (!isPatchingVfeStateAllowed) {
        return isFrontEndStateDirty * singleFrontEndCmdSize;
    }

    auto streamPropertiesCopy = csr->getStreamProperties();
    size_t estimatedSize = 0;

    for (size_t i = 0; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto &requiredStreamState = commandList->getRequiredStreamState();
        streamPropertiesCopy.frontEndState.setProperties(requiredStreamState.frontEndState);

        if (isFrontEndStateDirty || streamPropertiesCopy.frontEndState.isDirty()) {
            estimatedSize += singleFrontEndCmdSize;
            isFrontEndStateDirty = false;
        }
        auto &finalStreamState = commandList->getFinalStreamState();
        streamPropertiesCopy.frontEndState.setProperties(finalStreamState.frontEndState);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateStateComputeModeCmdSizeForMultipleCommandLists(
    uint32_t numCommandLists, ze_command_list_handle_t *phCommandLists) {

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    size_t singleScmCmdSize = NEO::EncodeComputeMode<GfxFamily>::getCmdSizeForComputeMode(this->device->getHwInfo(), false, csr->isRcs());

    auto streamPropertiesCopy = csr->getStreamProperties();
    size_t estimatedSize = 0;

    for (size_t i = 0; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto &requiredStreamState = commandList->getRequiredStreamState();
        streamPropertiesCopy.stateComputeMode.setProperties(requiredStreamState.stateComputeMode);

        if (streamPropertiesCopy.stateComputeMode.isDirty()) {
            estimatedSize += singleScmCmdSize;
        }
        auto &finalStreamState = commandList->getFinalStreamState();
        streamPropertiesCopy.stateComputeMode.setProperties(finalStreamState.stateComputeMode);
    }

    return estimatedSize;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimatePipelineSelect() {

    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    return NEO::PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(device->getHwInfo());
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programPipelineSelect(NEO::LinearStream &commandStream) {
    NEO::PipelineSelectArgs args = {0, 0};
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    NEO::PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, args, device->getHwInfo());
    gpgpuEnabled = true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::dispatchTaskCountWrite(NEO::LinearStream &commandStream, bool flushDataCache) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename PIPE_CONTROL::POST_SYNC_OPERATION;

    UNRECOVERABLE_IF(csr == nullptr);

    if (csr->isUpdateTagFromWaitEnabled()) {
        return;
    }

    auto taskCountToWrite = csr->peekTaskCount() + 1;
    auto gpuAddress = static_cast<uint64_t>(csr->getTagAllocation()->getGpuAddress());

    const auto &hwInfo = this->device->getHwInfo();
    if (isCopyOnlyCommandQueue) {
        NEO::MiFlushArgs args;
        args.commandWithPostSync = true;
        args.notifyEnable = csr->isUsedNotifyEnableForPostSync();
        NEO::EncodeMiFlushDW<GfxFamily>::programMiFlushDw(commandStream, gpuAddress, taskCountToWrite, args, hwInfo);
    } else {
        NEO::PipeControlArgs args;
        args.dcFlushEnable = NEO::MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
        if (partitionCount > 1) {
            args.workloadPartitionOffset = true;
        }
        args.notifyEnable = csr->isUsedNotifyEnableForPostSync();
        NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
            commandStream,
            POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
            gpuAddress,
            taskCountToWrite,
            hwInfo,
            args);
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandQueueHw<gfxCoreFamily>::getPreemptionCmdProgramming() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    return NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(NEO::PreemptionMode::MidThread, NEO::PreemptionMode::Initial) > 0u;
}

} // namespace L0
