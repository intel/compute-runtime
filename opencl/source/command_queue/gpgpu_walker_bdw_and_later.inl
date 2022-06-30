/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/simd_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/gpgpu_walker_base.inl"

namespace NEO {

template <typename GfxFamily>
inline size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WALKER_TYPE *walkerCmd,
    const KernelDescriptor &kernelDescriptor,
    const size_t globalOffsets[3],
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd,
    uint32_t workDim,
    bool localIdsGenerationByRuntime,
    bool inlineDataProgrammingRequired,
    uint32_t requiredWorkgroupOrder) {
    auto localWorkSize = localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2];

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    walkerCmd->setThreadWidthCounterMaximum(static_cast<uint32_t>(threadsPerWorkGroup));

    walkerCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    walkerCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    walkerCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = maxNBitValue(remainderSimdLanes);
    if (!executionMask)
        executionMask = ~executionMask;

    using SIMD_SIZE = typename WALKER_TYPE::SIMD_SIZE;

    walkerCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    walkerCmd->setSimdSize(getSimdConfig<WALKER_TYPE>(simd));

    walkerCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    walkerCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    walkerCmd->setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroups[2]));

    return localWorkSize;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WALKER_TYPE *walkerCmd,
    TagNodeBase *timestampPacketNode,
    const RootDeviceEnvironment &rootDeviceEnvironment) {

    uint64_t address = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        *cmdStream,
        PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        address,
        0,
        *rootDeviceEnvironment.getHardwareInfo(),
        args);

    EncodeDispatchKernel<GfxFamily>::adjustTimestampPacket(*walkerCmd, *rootDeviceEnvironment.getHardwareInfo());
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel, const DispatchInfo &dispatchInfo) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS() +
                  sizeof(PIPE_CONTROL) * (MemorySynchronizationCommands<GfxFamily>::isPipeControlWArequired(commandQueue.getDevice().getHardwareInfo()) ? 2 : 1);
    size += HardwareCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(commandQueue, pKernel, 0U);
    size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    size += PerformanceCounters::getGpuCommandsSize(commandQueue.getPerfCounters(), commandQueue.getGpgpuEngine().osContext->getEngineType(), reservePerfCounters);
    size += GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(pKernel);
    size += GpgpuWalkerHelper<GfxFamily>::getSizeForWaDisableRccRhwoOptimization(pKernel);

    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return sizeof(PIPE_CONTROL);
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::adjustMiStoreRegMemMode(MI_STORE_REG_MEM<GfxFamily> *storeCmd) {
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const HardwareInfo &hwInfo) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, GlobalStartTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        *commandStream,
        PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
        timeStampAddress,
        0llu,
        hwInfo,
        args);

    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
        //MI_STORE_REGISTER_MEM for context local timestamp
        timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextStartTS);

        //low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    TagNodeBase &hwTimeStamps,
    LinearStream *commandStream,
    const HardwareInfo &hwInfo) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, GlobalEndTS);
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        *commandStream,
        PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP,
        timeStampAddress,
        0llu,
        hwInfo,
        args);

    if (!HwHelper::get(hwInfo.platform.eRenderCoreFamily).useOnlyGlobalTimestamps()) {
        //MI_STORE_REGISTER_MEM for context local timestamp
        uint64_t timeStampAddress = hwTimeStamps.getGpuAddress() + offsetof(HwTimeStamps, ContextEndTS);

        //low part
        auto pMICmdLow = commandStream->getSpaceForCmd<MI_STORE_REGISTER_MEM>();
        MI_STORE_REGISTER_MEM cmd = GfxFamily::cmdInitStoreRegisterMem;
        adjustMiStoreRegMemMode(&cmd);
        cmd.setRegisterAddress(GP_THREAD_TIME_REG_ADDRESS_OFFSET_LOW);
        cmd.setMemoryAddress(timeStampAddress);
        *pMICmdLow = cmd;
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue) {
    return 0;
}

} // namespace NEO
