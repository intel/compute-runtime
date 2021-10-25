/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/command_queue/hardware_interface_base.inl"

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

    if (parentKernel) {
        dsh->getSpace(commandQueue.getContext().getDefaultDeviceQueue()->getDshOffset() - dsh->getUsed());
    }
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {
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

    using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;

    COMPUTE_WALKER walkerCmd = GfxFamily::cmdInitGpgpuWalker;
    auto &kernelInfo = kernel.getKernelInfo();

    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernelInfo.getMaxSimdSize();

    auto numChannels = kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;

    size_t globalOffsets[3] = {dispatchInfo.getOffset().x, dispatchInfo.getOffset().y, dispatchInfo.getOffset().z};
    size_t startWorkGroups[3] = {startOfWorkgroups.x, startOfWorkgroups.y, startOfWorkgroups.z};
    size_t numWorkGroups[3] = {numberOfWorkgroups.x, numberOfWorkgroups.y, numberOfWorkgroups.z};
    uint32_t requiredWalkOrder = 0u;

    bool localIdsGenerationByRuntime = EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
        numChannels,
        localWorkSizes,
        std::array<uint8_t, 3>{{kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[0],
                                kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[1],
                                kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]}},
        kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder,
        requiredWalkOrder,
        simd);

    bool inlineDataProgrammingRequired = HardwareCommandsHelper<GfxFamily>::inlineDataProgrammingRequired(kernel);
    auto idd = &walkerCmd.getInterfaceDescriptor();
    auto &queueCsr = commandQueue.getGpgpuCommandStreamReceiver();

    if (currentTimestampPacketNodes && queueCsr.peekTimestampPacketWriteEnabled()) {
        auto timestampPacket = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
        GpgpuWalkerHelper<GfxFamily>::setupTimestampPacket(&commandStream, &walkerCmd, timestampPacket, commandQueue.getDevice().getRootDeviceEnvironment());
    }

    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);

    if (auto kernelAllocation = kernelInfo.getGraphicsAllocation()) {
        EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(commandStream, *kernelAllocation, kernelInfo.heapInfo.KernelHeapSize, 0, commandQueue.getDevice().getHardwareInfo());
    }

    HardwareCommandsHelper<GfxFamily>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        kernel.getKernelStartOffset(localIdsGenerationByRuntime, kernelUsesLocalIds, isCcsUsed),
        simd,
        localWorkSizes,
        offsetInterfaceDescriptorTable,
        interfaceDescriptorIndex,
        preemptionMode,
        &walkerCmd,
        idd,
        localIdsGenerationByRuntime,
        commandQueue.getDevice());

    GpgpuWalkerHelper<GfxFamily>::setGpgpuWalkerThreadData(&walkerCmd, kernelInfo.kernelDescriptor, globalOffsets, startWorkGroups,
                                                           numWorkGroups, localWorkSizes, simd, dim,
                                                           localIdsGenerationByRuntime, inlineDataProgrammingRequired, requiredWalkOrder);

    EncodeDispatchKernel<GfxFamily>::encodeAdditionalWalkerFields(commandQueue.getDevice().getHardwareInfo(), walkerCmd);

    auto devices = queueCsr.getOsContext().getDeviceBitfield();
    auto partitionWalker = ImplicitScalingHelper::isImplicitScalingEnabled(devices, !kernel.isSingleSubdevicePreferred());

    if (partitionWalker) {
        const uint64_t workPartitionAllocationGpuVa = commandQueue.getDevice().getDefaultEngine().commandStreamReceiver->getWorkPartitionAllocationGpuAddress();
        uint32_t partitionCount = 0u;
        ImplicitScalingDispatch<GfxFamily>::dispatchCommands(commandStream,
                                                             walkerCmd,
                                                             devices,
                                                             partitionCount,
                                                             false,
                                                             false,
                                                             kernel.usesImages(),
                                                             workPartitionAllocationGpuVa);
        if (queueCsr.isStaticWorkPartitioningEnabled()) {
            queueCsr.setActivePartitions(std::max(queueCsr.getActivePartitions(), partitionCount));
        }
        auto timestampPacket = currentTimestampPacketNodes->peekNodes().at(currentDispatchIndex);
        timestampPacket->setPacketsUsed(partitionCount);
    } else {
        auto computeWalkerOnStream = commandStream.getSpaceForCmd<typename GfxFamily::COMPUTE_WALKER>();
        *computeWalkerOnStream = walkerCmd;
    }
}
} // namespace NEO
