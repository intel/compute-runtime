/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/gpgpu_walker.h"

namespace NEO {
template <typename GfxFamily>
void GpgpuWalkerHelper<GfxFamily>::dispatchScheduler(
    LinearStream &commandStream,
    DeviceQueueHw<GfxFamily> &devQueueHw,
    PreemptionMode preemptionMode,
    SchedulerKernel &scheduler,
    IndirectHeap *ssh,
    IndirectHeap *dsh,
    bool isCcsUsed) {

    const auto &kernelInfo = scheduler.getKernelInfo();

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
    uint32_t simd = kernelInfo.getMaxSimdSize();
    DEBUG_BREAK_IF(simd != PARALLEL_SCHEDULER_COMPILATION_SIZE_20);

    // Patch our kernel constants
    scheduler.setGlobalWorkOffsetValues(0, 0, 0);
    scheduler.setGlobalWorkSizeValues(static_cast<uint32_t>(scheduler.getGws()), 1, 1);
    scheduler.setLocalWorkSizeValues(static_cast<uint32_t>(scheduler.getLws()), 1, 1);
    scheduler.setLocalWorkSize2Values(static_cast<uint32_t>(scheduler.getLws()), 1, 1);
    scheduler.setEnqueuedLocalWorkSizeValues(static_cast<uint32_t>(scheduler.getLws()), 1, 1);
    scheduler.setNumWorkGroupsValues(static_cast<uint32_t>(scheduler.getGws() / scheduler.getLws()), 0, 0);
    scheduler.setWorkDim(1);

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
        true,
        devQueueHw.getDevice());

    // Implement enabling special WA DisableLSQCROPERFforOCL if needed
    GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(&commandStream, scheduler, true);

    size_t globalOffsets[3] = {0, 0, 0};
    size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};
    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&cmdWalker, kernelInfo.kernelDescriptor, globalOffsets, globalOffsets, workGroups, localWorkSizes,
                                                           simd, 1, true, inlineDataProgrammingRequired, 0u);
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
        cmdBbStart.setBatchBufferStartAddress(slbAddress);
        *bbStart = cmdBbStart;
    }
}
} // namespace NEO