/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/timestamp_packet.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/event/user_event.h"
#include "opencl/source/helpers/queue_helpers.h"

namespace NEO {

template <typename GfxFamily>
template <typename WalkerType>
void GpgpuWalkerHelper<GfxFamily>::setSystolicModeEnable(WalkerType *walkerCmd) {
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(
    CommandQueue &commandQueue,
    TagNodeBase &hwPerfCounter,
    LinearStream *commandStream) {

    const auto pPerformanceCounters = commandQueue.getPerfCounters();
    const auto commandBufferType = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType())
                                       ? MetricsLibraryApi::GpuCommandBufferType::Compute
                                       : MetricsLibraryApi::GpuCommandBufferType::Render;
    const uint32_t size = pPerformanceCounters->getGpuCommandsSize(commandBufferType, true);
    void *pBuffer = commandStream->getSpace(size);

    pPerformanceCounters->getGpuCommands(commandBufferType, hwPerfCounter, true, size, pBuffer);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(
    CommandQueue &commandQueue,
    TagNodeBase &hwPerfCounter,
    LinearStream *commandStream) {

    const auto pPerformanceCounters = commandQueue.getPerfCounters();
    const auto commandBufferType = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType())
                                       ? MetricsLibraryApi::GpuCommandBufferType::Compute
                                       : MetricsLibraryApi::GpuCommandBufferType::Render;
    const uint32_t size = pPerformanceCounters->getGpuCommandsSize(commandBufferType, false);
    void *pBuffer = commandStream->getSpace(size);

    pPerformanceCounters->getGpuCommands(commandBufferType, hwPerfCounter, false, size, pBuffer);
}

template <typename GfxFamily>
size_t GpgpuWalkerHelper<GfxFamily>::getSizeForWaDisableRccRhwoOptimization(const Kernel *pKernel) {
    return 0u;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getTotalSizeRequiredCS(uint32_t eventType, const CsrDependencies &csrDeps, bool reserveProfilingCmdsSpace, bool reservePerfCounters, bool blitEnqueue, CommandQueue &commandQueue, const MultiDispatchInfo &multiDispatchInfo, bool isMarkerWithProfiling, bool eventsInWaitlist, bool resolveDependenciesByPipecontrol, cl_event *outEvent) {
    size_t expectedSizeCS = 0;
    auto &gfxCoreHelper = commandQueue.getDevice().getGfxCoreHelper();

    auto &commandQueueHw = static_cast<CommandQueueHw<GfxFamily> &>(commandQueue);
    auto &rootDeviceEnvironment = commandQueue.getDevice().getRootDeviceEnvironment();
    auto isCacheFlushForBcs = commandQueueHw.isCacheFlushForBcsRequired();
    if (blitEnqueue) {
        size_t expectedSizeCS = TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<GfxFamily>();
        if (isCacheFlushForBcs || commandQueueHw.isCacheFlushForImageRequired(eventType)) {
            expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
        }

        return expectedSizeCS;
    }

    for (auto &dispatchInfo : multiDispatchInfo) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredCS(eventType, reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, dispatchInfo.getKernel(), dispatchInfo);
        size_t kernelObjAuxCount = multiDispatchInfo.getKernelObjsForAuxTranslation() != nullptr ? multiDispatchInfo.getKernelObjsForAuxTranslation()->size() : 0;
        expectedSizeCS += dispatchInfo.dispatchInitCommands.estimateCommandsSize(kernelObjAuxCount, rootDeviceEnvironment, isCacheFlushForBcs);
        expectedSizeCS += dispatchInfo.dispatchEpilogueCommands.estimateCommandsSize(kernelObjAuxCount, rootDeviceEnvironment, isCacheFlushForBcs);
    }

    auto relaxedOrderingEnabled = commandQueue.getGpgpuCommandStreamReceiver().directSubmissionRelaxedOrderingEnabled();

    if (relaxedOrderingEnabled) {
        expectedSizeCS += 2 * EncodeSetMMIO<GfxFamily>::sizeREG;
    }

    if (commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        // add relaxed ordering cond_bb_start
        expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSize<GfxFamily>(csrDeps, relaxedOrderingEnabled);
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite();
        if (resolveDependenciesByPipecontrol) {
            expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForStallingBarrier();
        }
        if (isMarkerWithProfiling) {
            if (!eventsInWaitlist) {
                expectedSizeCS += commandQueue.getGpgpuCommandStreamReceiver().getCmdsSizeForComputeBarrierCommand();
            }
            expectedSizeCS += 4 * EncodeStoreMMIO<GfxFamily>::size;
        }
    } else if (isMarkerWithProfiling) {
        expectedSizeCS += 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
        if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
            expectedSizeCS += 2 * EncodeStoreMMIO<GfxFamily>::size;
        }
    }
    if (multiDispatchInfo.peekMainKernel()) {
        expectedSizeCS += EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(*multiDispatchInfo.peekMainKernel(), commandQueue);
    }

    if (debugManager.flags.PauseOnEnqueue.get() != -1) {
        expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() * 2;
        expectedSizeCS += NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait() * 2;
    }

    if (debugManager.flags.GpuScratchRegWriteAfterWalker.get() != -1) {
        expectedSizeCS += sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    }
    expectedSizeCS += TimestampPacketHelper::getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer<GfxFamily>(csrDeps);
    if (outEvent) {
        auto pEvent = castToObjectOrAbort<Event>(*outEvent);
        if ((pEvent->getContext()->getRootDeviceIndices().size() > 1) && (!pEvent->isUserEvent())) {
            expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
        }
    }
    expectedSizeCS += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();

    if ((CL_COMMAND_BARRIER == eventType) && !commandQueue.isOOQEnabled() && eventsInWaitlist) {
        expectedSizeCS += EncodeStoreMemory<GfxFamily>::getStoreDataImmSize();
    }

    return expectedSizeCS;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCS(uint32_t cmdType, bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    if (isCommandWithoutKernel(cmdType)) {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue);
    } else {
        return EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel<typename GfxFamily::DefaultWalkerType>(reserveProfilingCmdsSpace, reservePerfCounters, commandQueue, pKernel, dispatchInfo);
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSNonKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue) {
    size_t size = 0;
    if (reserveProfilingCmdsSpace) {
        size += 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() + 4 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }

    return size;
}

template <typename GfxFamily>
template <typename WalkerType>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacketFlushL3(WalkerType &walkerCmd, CommandQueue &commandQueue, const FlushL3Args &args) {
}
} // namespace NEO
