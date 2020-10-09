/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/simd_helper.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/command_queue/gpgpu_walker_base.inl"

#include "pipe_control_args.h"

namespace NEO {

template <typename GfxFamily>
inline size_t GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(
    WALKER_TYPE<GfxFamily> *walkerCmd,
    const size_t globalOffsets[3],
    const size_t startWorkGroups[3],
    const size_t numWorkGroups[3],
    const size_t localWorkSizesIn[3],
    uint32_t simd,
    uint32_t workDim,
    bool localIdsGenerationByRuntime,
    bool inlineDataProgrammingRequired,
    const iOpenCL::SPatchThreadPayload &threadPayload,
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

    using SIMD_SIZE = typename WALKER_TYPE<GfxFamily>::SIMD_SIZE;

    walkerCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    walkerCmd->setSimdSize(getSimdConfig<WALKER_TYPE<GfxFamily>>(simd));

    walkerCmd->setThreadGroupIdStartingX(static_cast<uint32_t>(startWorkGroups[0]));
    walkerCmd->setThreadGroupIdStartingY(static_cast<uint32_t>(startWorkGroups[1]));
    walkerCmd->setThreadGroupIdStartingResumeZ(static_cast<uint32_t>(startWorkGroups[2]));

    return localWorkSize;
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
    LinearStream &commandStream,
    DeviceQueueHw<GfxFamily> &devQueueHw,
    PreemptionMode preemptionMode,
    SchedulerKernel &scheduler,
    IndirectHeap *ssh,
    IndirectHeap *dsh,
    bool isCcsUsed) {

    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    NEO::PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);

    uint32_t interfaceDescriptorIndex = devQueueHw.schedulerIDIndex;
    const size_t offsetInterfaceDescriptorTable = devQueueHw.colorCalcStateSize;
    const size_t offsetInterfaceDescriptor = offsetInterfaceDescriptorTable;
    const size_t totalInterfaceDescriptorTableSize = devQueueHw.interfaceDescriptorEntries * sizeof(INTERFACE_DESCRIPTOR_DATA);

    // Program media interface descriptor load
    HardwareCommandsHelper<GfxFamily>::sendMediaInterfaceDescriptorLoad(
        commandStream,
        offsetInterfaceDescriptor,
        totalInterfaceDescriptorTableSize);

    DEBUG_BREAK_IF(offsetInterfaceDescriptorTable % 64 != 0);

    // Determine SIMD size
    uint32_t simd = scheduler.getKernelInfo().getMaxSimdSize();
    DEBUG_BREAK_IF(simd != PARALLEL_SCHEDULER_COMPILATION_SIZE_20);

    // Patch our kernel constants
    *scheduler.globalWorkOffsetX = 0;
    *scheduler.globalWorkOffsetY = 0;
    *scheduler.globalWorkOffsetZ = 0;

    *scheduler.globalWorkSizeX = (uint32_t)scheduler.getGws();
    *scheduler.globalWorkSizeY = 1;
    *scheduler.globalWorkSizeZ = 1;

    *scheduler.localWorkSizeX = (uint32_t)scheduler.getLws();
    *scheduler.localWorkSizeY = 1;
    *scheduler.localWorkSizeZ = 1;

    *scheduler.localWorkSizeX2 = (uint32_t)scheduler.getLws();
    *scheduler.localWorkSizeY2 = 1;
    *scheduler.localWorkSizeZ2 = 1;

    *scheduler.enqueuedLocalWorkSizeX = (uint32_t)scheduler.getLws();
    *scheduler.enqueuedLocalWorkSizeY = 1;
    *scheduler.enqueuedLocalWorkSizeZ = 1;

    *scheduler.numWorkGroupsX = (uint32_t)(scheduler.getGws() / scheduler.getLws());
    *scheduler.numWorkGroupsY = 0;
    *scheduler.numWorkGroupsZ = 0;

    *scheduler.workDim = 1;

    // Send our indirect object data
    size_t localWorkSizes[3] = {scheduler.getLws(), 1, 1};

    // Create indirectHeap for IOH that is located at the end of device enqueue DSH
    size_t curbeOffset = devQueueHw.setSchedulerCrossThreadData(scheduler);
    IndirectHeap indirectObjectHeap(dsh->getCpuBase(), dsh->getMaxAvailableSpace());
    indirectObjectHeap.getSpace(curbeOffset);
    IndirectHeap *ioh = &indirectObjectHeap;

    // Program the walker.  Invokes execution so all state should already be programmed
    auto pGpGpuWalkerCmd = commandStream.getSpaceForCmd<GPGPU_WALKER>();
    GPGPU_WALKER cmdWalker = GfxFamily::cmdInitGpgpuWalker;

    bool inlineDataProgrammingRequired = HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(scheduler);
    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(scheduler);

    HardwareCommandsHelper<GfxFamily>::sendIndirectState(
        commandStream,
        *dsh,
        *ioh,
        *ssh,
        scheduler,
        scheduler.getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        &cmdWalker,
        nullptr,
        true);

    // Implement enabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&commandStream, scheduler, true);

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};
    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&cmdWalker, globalOffsets, globalOffsets, workGroups, localWorkSizes,
                                                           simd, 1, true, inlineDataProgrammingRequired,
                                                           *scheduler.getKernelInfo().patchInfo.threadPayload, 0u);
    *pGpGpuWalkerCmd = cmdWalker;

    // Implement disabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&commandStream, scheduler, false);

    // Do not put BB_START only when returning in first Scheduler run
    if (devQueueHw.getSchedulerReturnInstance() != 1) {
        args.dcFlushEnable = true;
        MemorySynchronizationCommands<GfxFamily>::addPipeControl(commandStream, args);

        // Add BB Start Cmd to the SLB in the Primary Batch Buffer
        auto bbStart = commandStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
        MI_BATCH_BUFFER_START cmdBbStart = GfxFamily::cmdInitBatchBufferStart;
        cmdBbStart.setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH);
        uint64_t slbAddress = devQueueHw.getSlbBuffer()->getGpuAddress();
        cmdBbStart.setBatchBufferStartAddressGraphicsaddress472(slbAddress);
        *bbStart = cmdBbStart;
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WALKER_TYPE<GfxFamily> *walkerCmd,
    TagNode<TimestampPacketStorage> *timestampPacketNode,
    TimestampPacketStorage::WriteOperationType writeOperationType,
    const RootDeviceEnvironment &rootDeviceEnvironment) {

    if (TimestampPacketStorage::WriteOperationType::AfterWalker == writeOperationType) {
        uint64_t address = TimestampPacketHelper::getContextEndGpuAddress(*timestampPacketNode);
        PipeControlArgs args;
        MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
            *cmdStream,
            PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
            address,
            0,
            *rootDeviceEnvironment.getHardwareInfo(),
            args);
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS(pKernel) +
                  sizeof(PIPE_CONTROL) * (MemorySynchronizationCommands<GfxFamily>::isPipeControlWArequired(pKernel->getDevice().getHardwareInfo()) ? 2 : 1);
    size += HardwareCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(commandQueue, pKernel, 0U);
    size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    size += PerformanceCounters::getGpuCommandsSize(commandQueue, reservePerfCounters);
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
    TagNode<HwTimeStamps> &hwTimeStamps,
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

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(
    TagNode<HwTimeStamps> &hwTimeStamps,
    LinearStream *commandStream,
    const HardwareInfo &hwInfo) {
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;

    // PIPE_CONTROL for global timestamp
    auto pPipeControlCmd = commandStream->getSpaceForCmd<PIPE_CONTROL>();
    PIPE_CONTROL cmdPipeControl = GfxFamily::cmdInitPipeControl;
    cmdPipeControl.setCommandStreamerStallEnable(true);
    *pPipeControlCmd = cmdPipeControl;

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

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeForCacheFlushAfterWalkerCommands(const Kernel &kernel, const CommandQueue &commandQueue) {
    return 0;
}

} // namespace NEO
