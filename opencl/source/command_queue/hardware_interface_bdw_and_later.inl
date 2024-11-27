/*
 * Copyright (C) 2019-2024 Intel Corporation
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
template <typename WalkerType>
inline void HardwareInterface<GfxFamily>::programWalker(
    LinearStream &commandStream,
    Kernel &kernel,
    CommandQueue &commandQueue,
    IndirectHeap &dsh,
    IndirectHeap &ioh,
    IndirectHeap &ssh,
    const DispatchInfo &dispatchInfo,
    HardwareInterfaceWalkerArgs &walkerArgs) {

    auto walkerCmdBuf = allocateWalkerSpace<WalkerType>(commandStream, kernel);
    WalkerType walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernel.getKernelInfo().getMaxSimdSize();
    auto &rootDeviceEnvironment = commandQueue.getDevice().getRootDeviceEnvironment();

    size_t startWorkGroups[3] = {walkerArgs.startOfWorkgroups->x, walkerArgs.startOfWorkgroups->y, walkerArgs.startOfWorkgroups->z};
    size_t numWorkGroups[3] = {walkerArgs.numberOfWorkgroups->x, walkerArgs.numberOfWorkgroups->y, walkerArgs.numberOfWorkgroups->z};
    auto threadGroupCount = static_cast<uint32_t>(walkerArgs.numberOfWorkgroups->x * walkerArgs.numberOfWorkgroups->y * walkerArgs.numberOfWorkgroups->z);

    if (walkerArgs.currentTimestampPacketNodes && walkerArgs.currentTimestampPacketNodes->peekNodes().size() > 0 &&
        commandQueue.getGpgpuCommandStreamReceiver().peekTimestampPacketWriteEnabled()) {
        auto timestampPacketNode = walkerArgs.currentTimestampPacketNodes->peekNodes().at(walkerArgs.currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, &walkerCmd, timestampPacketNode, rootDeviceEnvironment);
    }

    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);

    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&walkerCmd, kernel.getKernelInfo().kernelDescriptor,
                                                           startWorkGroups,
                                                           numWorkGroups, walkerArgs.localWorkSizes, simd, dim,
                                                           false, false, 0u);

    HardwareCommandsHelper<GfxFamily>::template sendIndirectState<WalkerType, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        kernel.getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        simd,
        walkerArgs.localWorkSizes,
        threadGroupCount,
        walkerArgs.offsetInterfaceDescriptorTable,
        walkerArgs.interfaceDescriptorIndex,
        walkerArgs.preemptionMode,
        &walkerCmd,
        nullptr,
        kernelUsesLocalIds,
        0,
        commandQueue.getDevice());

    EncodeWalkerArgs encodeWalkerArgs{
        kernel.getKernelInfo().kernelDescriptor, // kernelDescriptor
        kernel.getExecutionType(),               // kernelExecutionType
        RequiredDispatchWalkOrder::none,         // requiredDispatchWalkOrder
        0,                                       // localRegionSize
        0,                                       // maxFrontEndThreads
        false};                                  // requiredSystemFence
    EncodeDispatchKernel<GfxFamily>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, encodeWalkerArgs);
    *walkerCmdBuf = walkerCmd;
}
} // namespace NEO
