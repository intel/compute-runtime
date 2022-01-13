/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/os_context.h"

#include "opencl/source/command_queue/hardware_interface_base.inl"

namespace NEO {

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::getDefaultDshSpace(
    const size_t &offsetInterfaceDescriptorTable,
    CommandQueue &commandQueue,
    const MultiDispatchInfo &multiDispatchInfo,
    size_t &totalInterfaceDescriptorTableSize,
    IndirectHeap *dsh,
    LinearStream *commandStream) {

    size_t numDispatches = multiDispatchInfo.size();
    totalInterfaceDescriptorTableSize *= numDispatches;

    dsh->getSpace(totalInterfaceDescriptorTableSize);
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
    const Vec3<size_t> &numberOfWorkgroups,
    const Vec3<size_t> &startOfWorkgroups) {

    auto walkerCmdBuf = allocateWalkerSpace(commandStream, kernel);
    WALKER_TYPE walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();

    size_t globalOffsets[3] = {dispatchInfo.getOffset().x, dispatchInfo.getOffset().y, dispatchInfo.getOffset().z};
    size_t startWorkGroups[3] = {startOfWorkgroups.x, startOfWorkgroups.y, startOfWorkgroups.z};
    size_t numWorkGroups[3] = {numberOfWorkgroups.x, numberOfWorkgroups.y, numberOfWorkgroups.z};

    if (currentTimestampPacketNodes && commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNode = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, &walkerCmd, timestampPacketNode, commandQueue.getDevice().getRootDeviceEnvironment());
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
        &walkerCmd,
        nullptr,
        true,
        commandQueue.getDevice());

    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&walkerCmd, kernel.getKernelInfo().kernelDescriptor,
                                                           globalOffsets, startWorkGroups,
                                                           numWorkGroups, localWorkSizes, simd, dim,
                                                           false, false, 0u);

    EncodeDispatchKernel<GfxFamily>::encodeAdditionalWalkerFields(commandQueue.getDevice().getHardwareInfo(), walkerCmd, kernel.getExecutionType());
    *walkerCmdBuf = walkerCmd;
}
} // namespace NEO
