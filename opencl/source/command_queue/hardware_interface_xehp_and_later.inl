/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/scratch_space_controller.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
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
    IndirectHeap *dsh,
    LinearStream *commandStream) {
}

template <typename GfxFamily>
inline void HardwareInterface<GfxFamily>::dispatchWorkarounds(
    LinearStream *commandStream,
    CommandQueue &commandQueue,
    Kernel &kernel,
    const bool &enable) {
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

    using InterfaceDescriptorType = typename WalkerType::InterfaceDescriptorType;
    WalkerType walkerCmd = GfxFamily::template getInitGpuWalker<WalkerType>();

    auto &kernelInfo = kernel.getKernelInfo();

    uint32_t dim = dispatchInfo.getDim();
    uint32_t simd = kernelInfo.getMaxSimdSize();

    auto numChannels = kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;

    size_t globalOffsets[3] = {dispatchInfo.getOffset().x, dispatchInfo.getOffset().y, dispatchInfo.getOffset().z};
    size_t startWorkGroups[3] = {walkerArgs.startOfWorkgroups->x, walkerArgs.startOfWorkgroups->y, walkerArgs.startOfWorkgroups->z};
    size_t numWorkGroups[3] = {walkerArgs.numberOfWorkgroups->x, walkerArgs.numberOfWorkgroups->y, walkerArgs.numberOfWorkgroups->z};
    auto threadGroupCount = static_cast<uint32_t>(walkerArgs.numberOfWorkgroups->x * walkerArgs.numberOfWorkgroups->y * walkerArgs.numberOfWorkgroups->z);
    uint32_t requiredWalkOrder = 0u;

    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);
    bool localIdsGenerationByRuntime = kernelUsesLocalIds && EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
                                                                 numChannels,
                                                                 walkerArgs.localWorkSizes,
                                                                 std::array<uint8_t, 3>{{kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[0],
                                                                                         kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[1],
                                                                                         kernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[2]}},
                                                                 kernelInfo.kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder,
                                                                 requiredWalkOrder,
                                                                 simd);

    bool inlineDataProgrammingRequired = EncodeDispatchKernel<GfxFamily>::inlineDataProgrammingRequired(kernel.getKernelInfo().kernelDescriptor);
    auto &queueCsr = commandQueue.getGpgpuCommandStreamReceiver();
    auto &device = commandQueue.getDevice();
    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();

    TagNodeBase *timestampPacketNode = nullptr;
    if (walkerArgs.currentTimestampPacketNodes && (walkerArgs.currentTimestampPacketNodes->peekNodes().size() > walkerArgs.currentDispatchIndex)) {
        timestampPacketNode = walkerArgs.currentTimestampPacketNodes->peekNodes()[walkerArgs.currentDispatchIndex];
    }

    if (timestampPacketNode) {
        GpgpuWalkerHelper<GfxFamily>::template setupTimestampPacket<WalkerType>(&commandStream, &walkerCmd, timestampPacketNode, rootDeviceEnvironment);
    }

    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());
    const auto &hwInfo = device.getHardwareInfo();
    constexpr bool heaplessModeEnabled = GfxFamily::template isHeaplessMode<WalkerType>();

    if constexpr (heaplessModeEnabled == false) {
        if (auto kernelAllocation = kernelInfo.getGraphicsAllocation()) {
            EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(commandStream, *kernelAllocation, kernelInfo.heapInfo.kernelHeapSize, 0, rootDeviceEnvironment);
        }
    }

    GpgpuWalkerHelper<GfxFamily>::template setGpgpuWalkerThreadData<WalkerType>(&walkerCmd, kernelInfo.kernelDescriptor, globalOffsets, startWorkGroups,
                                                                                numWorkGroups, walkerArgs.localWorkSizes, simd, dim,
                                                                                localIdsGenerationByRuntime, inlineDataProgrammingRequired, requiredWalkOrder);

    auto requiredScratchSlot0Size = queueCsr.getRequiredScratchSlot0Size();
    auto requiredScratchSlot1Size = queueCsr.getRequiredScratchSlot1Size();
    auto *defaultCsr = device.getDefaultEngine().commandStreamReceiver;

    uint64_t scratchAddress = 0u;
    EncodeDispatchKernel<GfxFamily>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, &ssh, *defaultCsr);

    auto interfaceDescriptor = &walkerCmd.getInterfaceDescriptor();

    HardwareCommandsHelper<GfxFamily>::template sendIndirectState<WalkerType, InterfaceDescriptorType>(
        commandStream,
        dsh,
        ioh,
        ssh,
        kernel,
        kernel.getKernelStartAddress(localIdsGenerationByRuntime, kernelUsesLocalIds, isCcsUsed, heaplessModeEnabled),
        simd,
        walkerArgs.localWorkSizes,
        threadGroupCount,
        walkerArgs.offsetInterfaceDescriptorTable,
        walkerArgs.interfaceDescriptorIndex,
        walkerArgs.preemptionMode,
        &walkerCmd,
        interfaceDescriptor,
        localIdsGenerationByRuntime,
        scratchAddress,
        device,
        walkerArgs.heaplessStateInitEnabled);

    bool kernelSystemAllocation = false;
    if (kernel.isBuiltIn) {
        kernelSystemAllocation = kernel.getDestinationAllocationInSystemMemory();
    } else {
        kernelSystemAllocation = kernel.isAnyKernelArgumentUsingSystemMemory();
    }
    bool requiredSystemFence = kernelSystemAllocation && walkerArgs.event != nullptr;
    auto maxFrontEndThreads = device.getDeviceInfo().maxFrontEndThreads;
    EncodeWalkerArgs encodeWalkerArgs{kernel.getExecutionType(), requiredSystemFence, kernelInfo.kernelDescriptor, NEO::RequiredDispatchWalkOrder::none, 0, maxFrontEndThreads};
    EncodeDispatchKernel<GfxFamily>::template encodeAdditionalWalkerFields<WalkerType>(rootDeviceEnvironment, walkerCmd, encodeWalkerArgs);

    auto devices = queueCsr.getOsContext().getDeviceBitfield();
    auto partitionWalker = ImplicitScalingHelper::isImplicitScalingEnabled(devices, true);

    if (timestampPacketNode && debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
        auto gpuVa = walkerArgs.currentTimestampPacketNodes->peekNodes()[walkerArgs.currentDispatchIndex]->getGpuAddress();
        printf("\nPID:%u, TSP used for Walker: 0x%" PRIX64 ", cmdBuffer pos: 0x%" PRIX64, SysCalls::getProcessId(), gpuVa, commandStream.getCurrentGpuAddressPosition());
    }

    if (partitionWalker) {
        const uint64_t workPartitionAllocationGpuVa = defaultCsr->getWorkPartitionAllocationGpuAddress();
        uint32_t partitionCount = 0u;
        ImplicitScalingDispatch<GfxFamily>::template dispatchCommands<WalkerType>(commandStream,
                                                                                  walkerCmd,
                                                                                  nullptr,
                                                                                  devices,
                                                                                  kernel.usesImages() ? RequiredPartitionDim::x : RequiredPartitionDim::none,
                                                                                  partitionCount,
                                                                                  false,
                                                                                  false,
                                                                                  queueCsr.getDcFlushSupport(),
                                                                                  kernel.isSingleSubdevicePreferred(),
                                                                                  workPartitionAllocationGpuVa,
                                                                                  hwInfo);
        if (queueCsr.isStaticWorkPartitioningEnabled()) {
            queueCsr.setActivePartitions(std::max(queueCsr.getActivePartitions(), partitionCount));
        }

        if (timestampPacketNode) {
            timestampPacketNode->setPacketsUsed(partitionCount);
        }
    } else {
        auto computeWalkerOnStream = commandStream.getSpaceForCmd<WalkerType>();
        *computeWalkerOnStream = walkerCmd;
    }
}
} // namespace NEO
