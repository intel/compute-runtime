/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/utilities/hw_timestamps.h"

#include "opencl/source/command_queue/gpgpu_walker_base.inl"

namespace NEO {

template <typename GfxFamily>
template <typename WalkerType>
inline size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WalkerType *walkerCmd,
    const KernelDescriptor &kernelDescriptor,
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd,
    uint32_t workDim,
    bool localIdsGenerationByRuntime,
    bool inlineDataProgrammingRequired,
    uint32_t requiredWorkgroupOrder) {
    auto localWorkSize = static_cast<uint32_t>(localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2]);

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    walkerCmd->setThreadWidthCounterMaximum(threadsPerWorkGroup);

    walkerCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    walkerCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    walkerCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = maxNBitValue(remainderSimdLanes);
    if (!executionMask)
        executionMask = ~executionMask;

    walkerCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    walkerCmd->setSimdSize(getSimdConfig<DefaultWalkerType>(simd));

    walkerCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    walkerCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    walkerCmd->setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroups[2]));

    return localWorkSize;
}

template <typename GfxFamily>
template <typename WalkerType>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WalkerType *walkerCmd,
    TagNodeBase *timestampPacketNode,
    const RootDeviceEnvironment &rootDeviceEnvironment) {

    uint64_t address = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *cmdStream,
        PostSyncMode::immediateData,
        address,
        0,
        rootDeviceEnvironment,
        args);
}

template <typename GfxFamily>
template <typename WalkerType>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  sizeof(PIPE_CONTROL) * (MemorySynchronizationCommands<GfxFamily>::isBarrierWaRequired(commandQueue.getDevice().getRootDeviceEnvironment()) ? 2 : 1);
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    size += PerformanceCounters::getGpuCommandsSize(commandQueue.getPerfCounters(), commandQueue.getGpgpuEngine().osContext->getEngineType(), reservePerfCounters);
    size += GpgpuWalkerHelper<GfxFamily>::getSizeForWaDisableRccRhwoOptimization(pKernel);

    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return sizeof(PIPE_CONTROL);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, globalStartTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *commandStream,
        PostSyncMode::timestamp,
        timeStampAddress,
        0llu,
        rootDeviceEnvironment,
        args);

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        // MI_STORE_REGISTER_MEM for context local timestamp
        timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, contextStartTS);

        // low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, globalEndTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        *commandStream,
        PostSyncMode::timestamp,
        timeStampAddress,
        0llu,
        rootDeviceEnvironment,
        args);

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    if (!gfxCoreHelper.useOnlyGlobalTimestamps()) {
        // MI_STORE_REGISTER_MEM for context local timestamp
        uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, contextEndTS);

        // low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue) {
    return 0;
}

} // namespace NEO
