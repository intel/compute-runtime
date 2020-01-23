/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/engine_node_helper.h"
#include "core/os_interface/os_context.h"
#include "runtime/command_queue/hardware_interface_base.inl"

namespace NEO {

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
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
    LinearStream *commandStream,
    CommandQueue &commandQueue) {

    // If hwTimeStampAlloc is passed (not nullptr), then we know that profiling is enabled
    if (hwTimeStamps != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchProfilingCommandsStart(*hwTimeStamps, commandStream, commandQueue.getDevice().getHardwareInfo());
    }
    if (hwPerfCounter != nullptr) {
        GpgpuWalkerHelper<GfxFamily>::dispatchPerfCountersCommandsStart(commandQueue, *hwPerfCounter, commandStream);
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchProfilingPerfEndCommands(
    TagNode<HwTimeStamps> *hwTimeStamps,
    TagNode<HwPerfCounter> *hwPerfCounter,
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
    size_t globalWorkSizes[3],
    size_t localWorkSizes[3],
    PreemptionMode preemptionMode,
    size_t currentDispatchIndex,
    uint32_t &interfaceDescriptorIndex,
    const DispatchInfo &dispatchInfo,
    size_t offsetInterfaceDescriptorTable,
    Vec3<size_t> &numberOfWorkgroups,
    Vec3<size_t> &startOfWorkgroups) {

    auto walkerCmd = allocateWalkerSpace(commandStream, kernel);
    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();

    size_t globalOffsets[3] = {dispatchInfo.getOffset().x, dispatchInfo.getOffset().y, dispatchInfo.getOffset().z};
    size_t startWorkGroups[3] = {startOfWorkgroups.x, startOfWorkgroups.y, startOfWorkgroups.z};
    size_t numWorkGroups[3] = {numberOfWorkgroups.x, numberOfWorkgroups.y, numberOfWorkgroups.z};

    if (currentTimestampPacketNodes && commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNode = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, walkerCmd, timestampPacketNode, TimestampPacketStorage::WriteOperationType::AfterWalker, commandQueue.getDevice().getHardwareInfo());
    }

    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);

    HardwareCommandsHelper<GfxFamily>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        kernel.getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        walkerCmd,
        nullptr,
        true);

    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(walkerCmd, globalOffsets, startWorkGroups,
                                                           numWorkGroups, localWorkSizes, simd, dim,
                                                           false, false,
                                                           *kernel.getKernelInfo().patchInfo.threadPayload, 0u);
}

} // namespace NEO
