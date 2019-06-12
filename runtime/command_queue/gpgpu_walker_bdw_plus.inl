/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/gpgpu_walker_base.inl"

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
    const iOpenCL::SPatchThreadPayload &threadPayload) {
    auto localWorkSize = localWorkSizesIn[0] * localWorkSizesIn[1] * localWorkSizesIn[2];

    auto threadsPerWorkGroup = getThreadsPerWG(simd, localWorkSize);
    walkerCmd->setThreadWidthCounterMaximum(static_cast<uint32_t>(threadsPerWorkGroup));

    walkerCmd->setThreadGroupIdXDimension(static_cast<uint32_t>(numWorkGroups[0]));
    walkerCmd->setThreadGroupIdYDimension(static_cast<uint32_t>(numWorkGroups[1]));
    walkerCmd->setThreadGroupIdZDimension(static_cast<uint32_t>(numWorkGroups[2]));

    // compute executionMask - to tell which SIMD lines are active within thread
    auto remainderSimdLanes = localWorkSize & (simd - 1);
    uint64_t executionMask = (1ull << remainderSimdLanes) - 1;
    if (!executionMask)
        executionMask = ~executionMask;

    using SIMD_SIZE = typename WALKER_TYPE<GfxFamily>::SIMD_SIZE;

    walkerCmd->setRightExecutionMask(static_cast<uint32_t>(executionMask));
    walkerCmd->setBottomExecutionMask(static_cast<uint32_t>(0xffffffff));
    walkerCmd->setSimdSize(static_cast<SIMD_SIZE>(simd >> 4));

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
    IndirectHeap *dsh) {

    using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename GfxFamily::GPGPU_WALKER;
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;

    bool dcFlush = false;
    PipeControlHelper<GfxFamily>::addPipeControl(commandStream, dcFlush);

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
    size_t globalWorkSizes[3] = {scheduler.getGws(), 1, 1};

    // Create indirectHeap for IOH that is located at the end of device enqueue DSH
    size_t curbeOffset = devQueueHw.setSchedulerCrossThreadData(scheduler);
    IndirectHeap indirectObjectHeap(dsh->getCpuBase(), dsh->getMaxAvailableSpace());
    indirectObjectHeap.getSpace(curbeOffset);
    IndirectHeap *ioh = &indirectObjectHeap;

    // Program the walker.  Invokes execution so all state should already be programmed
    auto pGpGpuWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pGpGpuWalkerCmd = GfxFamily::cmdInitGpgpuWalker;

    bool localIdsGenerationByRuntime = HardwareCommandsHelper<GfxFamily>::isRuntimeLocalIdsGenerationRequired(1, globalWorkSizes, localWorkSizes);
    bool inlineDataProgrammingRequired = HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(scheduler);
    HardwareCommandsHelper<GfxFamily>::sendIndirectState(
        commandStream,
        *dsh,
        *ioh,
        *ssh,
        scheduler,
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        pGpGpuWalkerCmd,
        nullptr,
        localIdsGenerationByRuntime);

    // Implement enabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&commandStream, scheduler, true);

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};
    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(pGpGpuWalkerCmd, globalOffsets, globalOffsets, workGroups, localWorkSizes,
                                                           simd, 1, localIdsGenerationByRuntime, inlineDataProgrammingRequired,
                                                           *scheduler.getKernelInfo().patchInfo.threadPayload);

    // Implement disabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&commandStream, scheduler, false);

    // Do not put BB_START only when returning in first Scheduler run
    if (devQueueHw.getSchedulerReturnInstance() != 1) {

        PipeControlHelper<GfxFamily>::addPipeControl(commandStream, true);

        // Add BB Start Cmd to the SLB in the Primary Batch Buffer
        auto *bbStart = static_cast<MI_BATCH_BUFFER_START *>(commandStream.getSpace(sizeof(MI_BATCH_BUFFER_START)));
        *bbStart = GfxFamily::cmdInitBatchBufferStart;
        bbStart->setSecondLevelBatchBuffer(MI_BATCH_BUFFER_START::SECOND_LEVEL_BATCH_BUFFER_FIRST_LEVEL_BATCH);
        uint64_t slbAddress = devQueueHw.getSlbBuffer()->getGpuAddress();
        bbStart->setBatchBufferStartAddressGraphicsaddress472(slbAddress);
    }
}

template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(
    LinearStream *cmdStream,
    WALKER_TYPE<GfxFamily> *walkerCmd,
    TagNode<TimestampPacketStorage> *timestampPacketNode,
    TimestampPacketStorage::WriteOperationType writeOperationType) {

    if (TimestampPacketStorage::WriteOperationType::AfterWalker == writeOperationType) {
        uint64_t address = timestampPacketNode->getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
        PipeControlHelper<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(cmdStream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA, address, 0, false);
    }
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredCSKernel(bool reserveProfilingCmdsSpace, bool reservePerfCounters, CommandQueue &commandQueue, const Kernel *pKernel) {
    size_t size = sizeof(typename GfxFamily::GPGPU_WALKER) + HardwareCommandsHelper<GfxFamily>::getSizeRequiredCS(pKernel) +
                  sizeof(PIPE_CONTROL) * (HardwareCommandsHelper<GfxFamily>::isPipeControlWArequired() ? 2 : 1);
    size += HardwareCommandsHelper<GfxFamily>::getSizeRequiredForCacheFlush(commandQueue, pKernel, 0U, 0U);
    size += PreemptionHelper::getPreemptionWaCsSize<GfxFamily>(commandQueue.getDevice());
    if (reserveProfilingCmdsSpace) {
        size += 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    if (reservePerfCounters) {
        //start cmds
        //P_C: flush CS & TimeStamp BEGIN
        size += 2 * sizeof(PIPE_CONTROL);
        //SRM NOOPID & Frequency
        size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        //gp registers
        size += NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        //report perf count
        size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
        //user registers
        size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);

        //end cmds
        //P_C: flush CS & TimeStamp END;
        size += 2 * sizeof(PIPE_CONTROL);
        //OA buffer (status head, tail)
        size += 3 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        //report perf count
        size += sizeof(typename GfxFamily::MI_REPORT_PERF_COUNT);
        //gp registers
        size += NEO::INSTR_GENERAL_PURPOSE_COUNTERS_COUNT * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        //SRM NOOPID & Frequency
        size += 2 * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
        //user registers
        size += commandQueue.getPerfCountersUserRegistersNumber() * sizeof(typename GfxFamily::MI_STORE_REGISTER_MEM);
    }
    size += GpgpuWalkerHelper<GfxFamily>::getSizeForWADisableLSQCROPERFforOCL(pKernel);

    return size;
}

template <typename GfxFamily>
size_t EnqueueOperation<GfxFamily>::getSizeRequiredForTimestampPacketWrite() {
    return sizeof(PIPE_CONTROL);
}

} // namespace NEO
