/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_queue/hardware_interface.h"

namespace OCLRT {

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    Kernel *parentKernel,
    IndirectHeap *dsh,
    LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    if (!parentKernel) {
        dsh->getSpace(totalInterfaceDescriptorTableSize);
    } else {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }
}

template <typename GfxFamily>
inline typename HardwareInterface<GfxFamily>::INTERFACE_DESCRIPTOR_DATA *
HardwareInterface<GfxFamily>::obtainInterfaceDescriptorData(
    WALKER_TYPE<GfxFamily> *walkerCmd) {
    return nullptr;
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {

    if (enable) {
        PreemptionHelper::applyPreemptionWaCmdsBegin<GfxFamily>(commandStream, commandQueue.getDevice());
        // Implement enabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
    } else {
        // Implement disabling special WA DisableLSQCROPERFforOCL if needed
        GpgpuWalkerHelper<GfxFamily>::applyWADisableLSQCROPERFforOCL(commandStream, kernel, enable);
        PreemptionHelper::applyPreemptionWaCmdsEnd<GfxFamily>(commandStream, commandQueue.getDevice());
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfStartCommands(
    const DispatchInfo &dispatchInfo,
    const MultiDispatchInfo &multiDispatchInfo,
    TagNode<HwTimeStamps> *hwTimeStamps,
    HwPerfCounter *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    if (&dispatchInfo == &*multiDispatchInfo.begin()) {
        // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
        if (hwTimeStamps != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream);
        }
        if (hwPerfCounter != nullptr) {
            GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
        }
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfEndCommands(
    TagNode<HwTimeStamps> *hwTimeStamps,
    HwPerfCounter *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStamps is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsEnd(*hwTimeStamps, commandStream);
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsEnd(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::programWalker(
    LinearStream &commandStream,
    Kernel &kernel,
    CommandQueue &commandQueue,
    TimestampPacketContainer *currentTimestampPacketNodes,
    IndirectHeap &dsh,
    IndirectHeap &ioh,
    IndirectHeap &ssh,
    size_t localWorkSizes[3],
    PreemptionMode preemptionMode,
    size_t currentDispatchIndex,
    uint32_t &interfaceDescriptorIndex,
    const DispatchInfo &dispatchInfo,
    size_t offsetInterfaceDescriptorTable) {

    auto walkerCmd = allocateWalkerSpace(commandStream, kernel);
    uint32_t dim = dispatchInfo.getDim();
    Vec3<size_t> lws = dispatchInfo.getLocalWorkgroupSize();
    Vec3<size_t> gws = dispatchInfo.getGWS();
    Vec3<size_t> swgs = dispatchInfo.getStartOfWorkgroups();
    Vec3<size_t> twgs = (dispatchInfo.getTotalNumberOfWorkgroups().x > 0) ? dispatchInfo.getTotalNumberOfWorkgroups() : generateWorkgroupsNumber(gws, lws);
    Vec3<size_t> nwgs = (dispatchInfo.getNumberOfWorkgroups().x > 0) ? dispatchInfo.getNumberOfWorkgroups() : twgs;
    size_t globalWorkSizes[3] = {gws.x, gws.y, gws.z};

    if (currentTimestampPacketNodes && commandQueue.getCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacket = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex)->tag;
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, walkerCmd, timestampPacket, TimestampPacket::WriteOperationType::AfterWalker);
    }

    auto idd = obtainInterfaceDescriptorData(walkerCmd);

    bool localIdsGenerationByRuntime = KernelCommandsHelper<GfxFamily>::isRuntimeLocalIdsGenerationRequired(dim, globalWorkSizes, localWorkSizes);
    bool inlineDataProgrammingRequired = KernelCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(kernel);
    bool kernelUsesLocalIds = KernelCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);
    uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();

    Vec3<size_t> offset = dispatchInfo.getOffset();

    KernelCommandsHelper<GfxFamily>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        walkerCmd,
        idd,
        localIdsGenerationByRuntime,
        kernelUsesLocalIds,
        inlineDataProgrammingRequired);

    size_t globalOffsets[3] = {offset.x, offset.y, offset.z};
    size_t startWorkGroups[3] = {swgs.x, swgs.y, swgs.z};
    size_t numWorkGroups[3] = {nwgs.x, nwgs.y, nwgs.z};
    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(walkerCmd, globalOffsets, startWorkGroups,
                                                           numWorkGroups, localWorkSizes, simd, dim,
                                                           localIdsGenerationByRuntime, inlineDataProgrammingRequired,
                                                           *kernel.getKernelInfo().patchInfo.threadPayload);

    GpgpuWalkerHelper<GfxFamily>::adjustWalkerData(&commandStream, walkerCmd, kernel, dispatchInfo);
}

} // namespace OCLRT
