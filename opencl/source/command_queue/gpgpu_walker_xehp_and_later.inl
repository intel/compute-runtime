/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_flush_xehp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/l3_range.h"
#include "shared/source/helpers/simd_helper.h"

#include "opencl/source/command_queue/gpgpu_walker_base.inl"
#include "opencl/source/platform/platform.h"

namespace NEO {

template <typename GfxFamily>
template <typename WalkerType>
size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WalkerType *walkerCmd,
    const KernelDescriptor &kernelDescriptor,
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd,
    uint32_t workDim,
    bool localIdsGenerationByRuntime,
    bool inlineDataProgrammingRequired,
    uint32_t requiredWorkGroupOrder) {

    bool kernelUsesLocalIds = kernelDescriptor.kernelAttributes.numLocalIdChannels > 0;

    auto localWorkSize = localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2];

    walkerCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    walkerCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    walkerCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = maxNBitValue(remainderSimdLanes);
    if (!executionMask) {
        executionMask = maxNBitValue(isSimd1(simd) ? 32 : simd);
    }

    walkerCmd->setExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setSimdSize(getSimdConfig<WalkerType>(simd));
    walkerCmd->setMessageSimd(walkerCmd->getSimdSize());

    if (debugManager.flags.ForceSimdMessageSizeInWalker.get() != -1) {
        walkerCmd->setMessageSimd(debugManager.flags.ForceSimdMessageSizeInWalker.get());
    }

    walkerCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    walkerCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    walkerCmd->setThreadGroupIdStartingZ(static_cast<uint32_t>(startWorkGroups[2]));

    // 1) cross-thread inline data will be put into R1, but if kernel uses local ids, then cross-thread should be put further back
    // so whenever local ids are driver or hw generated, reserve space by setting right values for emitLocalIds
    // 2) Auto-generation of local ids should be possible, when in fact local ids are used

    if (!localIdsGenerationByRuntime && kernelUsesLocalIds) {
        uint32_t emitLocalIdsForDim = 0;
        if (kernelDescriptor.kernelAttributes.localId[0]) {
            emitLocalIdsForDim |= (1 << 0);
        }
        if (kernelDescriptor.kernelAttributes.localId[1]) {
            emitLocalIdsForDim |= (1 << 1);
        }
        if (kernelDescriptor.kernelAttributes.localId[2]) {
            emitLocalIdsForDim |= (1 << 2);
        }
        walkerCmd->setEmitLocalId(emitLocalIdsForDim);
    }

    if (inlineDataProgrammingRequired == true) {
        walkerCmd->setEmitInlineParameter(1);
    }

    if ((!localIdsGenerationByRuntime) && kernelUsesLocalIds) {
        walkerCmd->setLocalXMaximum(static_cast<uint32_t>(localWorkSizesIn[0] - 1));
        walkerCmd->setLocalYMaximum(static_cast<uint32_t>(localWorkSizesIn[1] - 1));
        walkerCmd->setLocalZMaximum(static_cast<uint32_t>(localWorkSizesIn[2] - 1));

        walkerCmd->setGenerateLocalId(1);
        walkerCmd->setWalkOrder(requiredWorkGroupOrder);
    }

    return localWorkSize;
}

template <typename GfxFamily>
template <typename WalkerType>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(LinearStream *cmdStream,
                                                        WalkerType *walkerCmd,
                                                        TagNodeBase *timestampPacketNode,
                                                        const RootDeviceEnvironment &rootDeviceEnvironment) {
    using POSTSYNC_DATA = decltype(GfxFamily::template getPostSyncType<WalkerType>());

    auto &postSyncData = walkerCmd->getPostSync();
    postSyncData.setDataportPipelineFlush(true);
    postSyncData.setDataportSubsliceCacheFlush(true);

    if (NEO::debugManager.flags.ForcePostSyncL1Flush.get() != -1) {
        postSyncData.setDataportPipelineFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
        postSyncData.setDataportSubsliceCacheFlush(!!NEO::debugManager.flags.ForcePostSyncL1Flush.get());
    }

    auto mocs = EncodePostSync<GfxFamily>::getPostSyncMocs(rootDeviceEnvironment,
                                                           MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, rootDeviceEnvironment));

    postSyncData.setMocs(mocs);

    if (debugManager.flags.UseImmDataWriteModeOnPostSyncOperation.get()) {
        postSyncData.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_IMMEDIATE_DATA);
        auto contextEndAddress = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);
        postSyncData.setDestinationAddress(contextEndAddress);
        postSyncData.setImmediateData(0x2'0000'0002);
    } else {
        postSyncData.setOperation(POSTSYNC_DATA::OPERATION::OPERATION_WRITE_TIMESTAMP);
        auto contextStartAddress = TimestampPacketHelper::getContextStartGpuAddress(*timestampPacketNode);
        postSyncData.setDestinationAddress(contextStartAddress);
    }

    setSystolicModeEnable(walkerCmd);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<GfxFamily> *storeCmd) {
    storeCmd->setMmioRemapEnable(true);
}

template <typename GfxFamily>
template <typename WalkerType>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    size_t numBarriers = MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(commandQueue.getDevice().getRootDeviceEnvironment()) ? 2 : 1;

    size_t size = sizeof(WalkerType) +
                  (MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() * numBarriers) +
                  HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  EncodeMemoryPrefetch<GfxFamily>::getSizeForMemoryPrefetch(pKernel->getKernelInfo().heapInfo.kernelHeapSize, commandQueue.getDevice().getRootDeviceEnvironment());
    auto devices = commandQueue.getGpgpuCommandStreamReceiver().getOsContext().getDeviceBitfield();
    auto partitionWalker = ImplicitScalingHelper::isImplicitScalingEnabled(devices, true);
    if (partitionWalker) {
        Vec3<size_t> groupStart = dispatchInfo.getStartOfWorkgroups();
        Vec3<size_t> groupCount = dispatchInfo.getNumberOfWorkgroups();
        UNRECOVERABLE_IF(groupCount.x == 0);
        const bool staticPartitioning = commandQueue.getGpgpuCommandStreamReceiver().isStaticWorkPartitioningEnabled();
        size += static_cast<size_t>(ImplicitScalingDispatch<GfxFamily>::template getSize<WalkerType>(false, staticPartitioning, devices, groupStart, groupCount));
    }

    size += PerformanceCounters::getGpuCommandsSize(commandQueue.getPerfCounters(), commandQueue.getGpgpuEngine().osContext->getEngineType(), reservePerfCounters);

    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return 0;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(TagNodeBase &hwTimeStamps, LinearStream *commandStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(TagNodeBase &hwTimeStamps, LinearStream *commandStream, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue) {
    return 0;
}

} // namespace NEO
