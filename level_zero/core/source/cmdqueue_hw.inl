/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/interlocked_max.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "level_zero/core/source/cmdlist.h"
#include "level_zero/core/source/cmdlist_hw.h"
#include "level_zero/core/source/cmdqueue_hw.h"
#include "level_zero/core/source/device.h"
#include "level_zero/core/source/fence.h"
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
    delete commandStream;
    buffers.destroy(this->getDevice()->getDriverHandle()->getMemoryManager());
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

    size_t spaceForResidency = 0;
    size_t preemptionSize = 0u;
    constexpr size_t residencyContainerSpaceForPreemption = 2;
    constexpr size_t residencyContainerSpaceForFence = 1;
    constexpr size_t residencyContainerSpaceForTagWrite = 1;

    NEO::Device *neoDevice = device->getNEODevice();
    NEO::PreemptionMode statePreemption = commandQueuePreemptionMode;
    auto devicePreemption = device->getDevicePreemptionMode();
    if (commandQueuePreemptionMode == NEO::PreemptionMode::Initial) {
        preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(commandQueuePreemptionMode,
                                                                                     devicePreemption) +
                          NEO::PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*neoDevice) +
                          NEO::PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*neoDevice);
        statePreemption = devicePreemption;
    }
    if (devicePreemption == NEO::PreemptionMode::MidThread) {
        spaceForResidency += residencyContainerSpaceForPreemption;
    }

    bool directSubmissionEnabled = csr->isDirectSubmissionEnabled();

    NEO::ResidencyContainer residencyContainer;
    L0::Fence *fence = nullptr;

    device->activateMetricGroups();

    size_t totalCmdBuffers = 0;
    for (auto i = 0u; i < numCommandLists; i++) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);

        totalCmdBuffers += commandList->commandContainer.getCmdBufferAllocations().size();
        spaceForResidency += commandList->commandContainer.getResidencyContainer().size();
        auto commandListPreemption = commandList->getCommandListPreemptionMode();
        if (statePreemption != commandListPreemption) {
            preemptionSize += sizeof(PIPE_CONTROL);
            preemptionSize += NEO::PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(commandListPreemption, statePreemption);
            statePreemption = commandListPreemption;
        }

        interlockedMax(commandQueuePerThreadScratchSize, commandList->getCommandListPerThreadScratchSize());
    }

    size_t linearStreamSizeEstimate = totalCmdBuffers * sizeof(MI_BATCH_BUFFER_START);

    if (directSubmissionEnabled) {
        linearStreamSizeEstimate += sizeof(MI_BATCH_BUFFER_START);
    } else {
        linearStreamSizeEstimate += sizeof(MI_BATCH_BUFFER_END);
    }

    if (hFence) {
        fence = Fence::fromHandle(hFence);
        spaceForResidency += residencyContainerSpaceForFence;
        linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(device->getHwInfo());
    }

    spaceForResidency += residencyContainerSpaceForTagWrite;

    residencyContainer.reserve(spaceForResidency);

    auto scratchSpaceController = csr->getScratchSpaceController();
    bool gsbaStateDirty = false;
    bool frontEndStateDirty = false;
    handleScratchSpace(residencyContainer,
                       scratchSpaceController,
                       gsbaStateDirty, frontEndStateDirty);

    gsbaStateDirty |= !gsbaInit;
    frontEndStateDirty |= !frontEndInit;

    if (!gpgpuEnabled) {
        linearStreamSizeEstimate += estimatePipelineSelect();
    }

    if (frontEndStateDirty) {
        linearStreamSizeEstimate += estimateFrontEndCmdSize();
    }

    if (gsbaStateDirty) {
        linearStreamSizeEstimate += estimateStateBaseAddressCmdSize();
    }

    linearStreamSizeEstimate += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(device->getHwInfo());

    linearStreamSizeEstimate += preemptionSize;

    size_t alignedSize = alignUp<size_t>(linearStreamSizeEstimate, minCmdBufferPtrAlign);
    size_t padding = alignedSize - linearStreamSizeEstimate;
    reserveLinearStreamSize(alignedSize);
    NEO::LinearStream child(commandStream->getSpace(alignedSize), alignedSize);

    if (!gpgpuEnabled) {
        programPipelineSelect(child);
    }
    if (frontEndStateDirty) {
        programFrontEnd(scratchSpaceController->getScratchPatchAddress(), child);
    }
    if (gsbaStateDirty) {
        programGeneralStateBaseAddress(scratchSpaceController->calculateNewGSH(), child);
    }

    if (commandQueuePreemptionMode == NEO::PreemptionMode::Initial) {
        NEO::PreemptionHelper::programCsrBaseAddress<GfxFamily>(child, *neoDevice, csr->getPreemptionAllocation());
        NEO::PreemptionHelper::programStateSip<GfxFamily>(child, *neoDevice);
        NEO::PreemptionHelper::programCmdStream<GfxFamily>(child,
                                                           devicePreemption,
                                                           commandQueuePreemptionMode,
                                                           csr->getPreemptionAllocation());
        commandQueuePreemptionMode = devicePreemption;
        statePreemption = commandQueuePreemptionMode;
    }

    if (devicePreemption == NEO::PreemptionMode::MidThread) {
        residencyContainer.push_back(csr->getPreemptionAllocation());
        auto sipIsa = neoDevice->getBuiltIns()->getSipKernel(NEO::SipKernelType::Csr, *neoDevice).getSipAllocation();
        residencyContainer.push_back(sipIsa);
    }

    for (auto i = 0u; i < numCommandLists; ++i) {
        auto commandList = CommandList::fromHandle(phCommandLists[i]);
        auto cmdBufferAllocations = commandList->commandContainer.getCmdBufferAllocations();
        auto cmdBufferCount = cmdBufferAllocations.size();

        auto commandListPreemption = commandList->getCommandListPreemptionMode();
        if (statePreemption != commandListPreemption) {
            NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(child, false);
            NEO::PreemptionHelper::programCmdStream<GfxFamily>(child,
                                                               commandListPreemption,
                                                               statePreemption,
                                                               csr->getPreemptionAllocation());
            statePreemption = commandListPreemption;
        }

        for (size_t iter = 0; iter < cmdBufferCount; iter++) {
            auto allocation = cmdBufferAllocations[iter];
            NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&child, allocation->getGpuAddress(), true);
        }

        printfFunctionContainer.insert(printfFunctionContainer.end(),
                                       commandList->getPrintfFunctionContainer().begin(),
                                       commandList->getPrintfFunctionContainer().end());

        NEO::PageFaultManager *pageFaultManager = nullptr;
        if (performMigration) {
            pageFaultManager = device->getDriverHandle()->getMemoryManager()->getPageFaultManager();
            if (pageFaultManager == nullptr) {
                performMigration = false;
            }
        }

        for (auto alloc : commandList->commandContainer.getResidencyContainer()) {
            if (residencyContainer.end() ==
                std::find(residencyContainer.begin(), residencyContainer.end(), alloc)) {
                residencyContainer.push_back(alloc);

                if (performMigration) {
                    if (alloc &&
                        (alloc->getAllocationType() == NEO::GraphicsAllocation::AllocationType::SVM_GPU ||
                         alloc->getAllocationType() == NEO::GraphicsAllocation::AllocationType::SVM_CPU)) {
                        pageFaultManager->moveAllocationToGpuDomain(reinterpret_cast<void *>(alloc->getGpuAddress()));
                    }
                }
            }
        }
    }

    commandQueuePreemptionMode = statePreemption;

    if (hFence) {
        residencyContainer.push_back(&fence->getAllocation());
        NEO::MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
            child, POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
            fence->getGpuAddress(), Fence::STATE_SIGNALED, true, device->getHwInfo());
    }

    dispatchTaskCountWrite(child, true);
    residencyContainer.push_back(csr->getTagAllocation());
    void *endingCmd = nullptr;
    if (directSubmissionEnabled) {
        endingCmd = child.getSpace(0);
        NEO::EncodeBatchBufferStartOrEnd<GfxFamily>::programBatchBufferStart(&child, 0ull, false);
    } else {
        MI_BATCH_BUFFER_END cmd = GfxFamily::cmdInitBatchBufferEnd;
        auto buffer = child.getSpaceForCmd<MI_BATCH_BUFFER_END>();
        *(MI_BATCH_BUFFER_END *)buffer = cmd;
    }

    if (padding) {
        void *paddingPtr = child.getSpace(padding);
        memset(paddingPtr, 0, padding);
    }

    submitBatchBuffer(ptrDiff(child.getCpuBase(), commandStream->getCpuBase()), residencyContainer, endingCmd);

    this->taskCount = csr->peekTaskCount();

    csr->makeSurfacePackNonResident(residencyContainer);

    if (getSynchronousMode() == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS) {
        this->synchronize(std::numeric_limits<uint32_t>::max());
    }

    return ZE_RESULT_SUCCESS;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandQueueHw<gfxCoreFamily>::programFrontEnd(uint64_t scratchAddress, NEO::LinearStream &commandStream) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    UNRECOVERABLE_IF(csr == nullptr);
    NEO::PreambleHelper<GfxFamily>::programVFEState(&commandStream,
                                                    device->getHwInfo(),
                                                    commandQueuePerThreadScratchSize,
                                                    scratchAddress,
                                                    device->getMaxNumHwThreads(),
                                                    csr->getOsContext().getEngineType());
    frontEndInit = true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
size_t CommandQueueHw<gfxCoreFamily>::estimateFrontEndCmdSize() {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    return NEO::PreambleHelper<GfxFamily>::getVFECommandsSize();
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

    auto taskCountToWrite = csr->peekTaskCount() + 1;
    auto gpuAddress = static_cast<uint64_t>(csr->getTagAllocation()->getGpuAddress());

    NEO::MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
        commandStream, POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        gpuAddress, taskCountToWrite, true, device->getHwInfo());
}
} // namespace L0
