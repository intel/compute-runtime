/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

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

#include "encode_dispatch_kernel_args_ext.h"

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

    const auto &kernelAttributes = kernelInfo.kernelDescriptor.kernelAttributes;

    auto numChannels = kernelAttributes.numLocalIdChannels;

    size_t startWorkGroups[3] = {walkerArgs.startOfWorkgroups->x, walkerArgs.startOfWorkgroups->y, walkerArgs.startOfWorkgroups->z};
    size_t numWorkGroups[3] = {walkerArgs.numberOfWorkgroups->x, walkerArgs.numberOfWorkgroups->y, walkerArgs.numberOfWorkgroups->z};
    auto threadGroupCount = static_cast<uint32_t>(walkerArgs.numberOfWorkgroups->x * walkerArgs.numberOfWorkgroups->y * walkerArgs.numberOfWorkgroups->z);
    uint32_t requiredWalkOrder = 0u;

    auto kernelUsesLocalIds = HardwareCommandsHelper<GfxFamily>::kernelUsesLocalIds(kernel);
    bool localIdsGenerationByRuntime = kernelUsesLocalIds && EncodeDispatchKernel<GfxFamily>::isRuntimeLocalIdsGenerationRequired(
                                                                 numChannels,
                                                                 walkerArgs.localWorkSizes,
                                                                 std::array<uint8_t, 3>{{kernelAttributes.workgroupWalkOrder[0],
                                                                                         kernelAttributes.workgroupWalkOrder[1],
                                                                                         kernelAttributes.workgroupWalkOrder[2]}},
                                                                 kernelAttributes.flags.requiresWorkgroupWalkOrder,
                                                                 requiredWalkOrder,
                                                                 simd);

    bool inlineDataProgrammingRequired = EncodeDispatchKernel<GfxFamily>::inlineDataProgrammingRequired(kernel.getKernelInfo().kernelDescriptor);
    auto &queueCsr = commandQueue.getGpgpuCommandStreamReceiver();
    auto &device = commandQueue.getDevice();
    auto &rootDeviceEnvironment = device.getRootDeviceEnvironment();
    bool kernelSystemAllocation = kernel.isBuiltInKernel() ? kernel.getDestinationAllocationInSystemMemory()
                                                           : kernel.isAnyKernelArgumentUsingSystemMemory();

    TagNodeBase *timestampPacketNode = nullptr;
    if (walkerArgs.currentTimestampPacketNodes && (walkerArgs.currentTimestampPacketNodes->peekNodes().size() > walkerArgs.currentDispatchIndex)) {
        timestampPacketNode = walkerArgs.currentTimestampPacketNodes->peekNodes()[walkerArgs.currentDispatchIndex];
    }

    constexpr bool heaplessModeEnabled = GfxFamily::template isHeaplessMode<WalkerType>();

    if (timestampPacketNode) {

        GpgpuWalkerHelper<GfxFamily>::template setupTimestampPacket<WalkerType>(&commandStream, &walkerCmd, timestampPacketNode, rootDeviceEnvironment);

        if constexpr (heaplessModeEnabled) {
            auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
            if (productHelper.isL3FlushAfterPostSyncSupported(true)) {
                GpgpuWalkerHelper<GfxFamily>::setupTimestampPacketFlushL3(walkerCmd,
                                                                          commandQueue,
                                                                          FlushL3Args{.containsPrintBuffer = kernel.hasPrintfOutput(),
                                                                                      .usingSharedObjects = kernel.isUsingSharedObjArgs(),
                                                                                      .signalEvent = walkerArgs.event != nullptr,
                                                                                      .blocking = walkerArgs.blocking,
                                                                                      .usingSystemAllocation = kernelSystemAllocation});
            }
        }
    }
    auto isCcsUsed = EngineHelpers::isCcs(commandQueue.getGpgpuEngine().osContext->getEngineType());

    if constexpr (heaplessModeEnabled == false) {
        if (auto kernelAllocation = kernelInfo.getGraphicsAllocation()) {
            EncodeMemoryPrefetch<GfxFamily>::programMemoryPrefetch(commandStream, *kernelAllocation, kernelInfo.heapInfo.kernelHeapSize, 0, rootDeviceEnvironment);
        }
    }

    GpgpuWalkerHelper<GfxFamily>::template setGpgpuWalkerThreadData<WalkerType>(&walkerCmd, kernelInfo.kernelDescriptor, startWorkGroups,
                                                                                numWorkGroups, walkerArgs.localWorkSizes, simd, dim,
                                                                                localIdsGenerationByRuntime, inlineDataProgrammingRequired, requiredWalkOrder);

    auto requiredScratchSlot0Size = queueCsr.getRequiredScratchSlot0Size();
    auto requiredScratchSlot1Size = queueCsr.getRequiredScratchSlot1Size();

    uint64_t scratchAddress = 0u;

    EncodeDispatchKernel<GfxFamily>::template setScratchAddress<heaplessModeEnabled>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, &ssh, queueCsr);

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
        device);

    EncodeKernelArgsExt argsExtended{};

    EncodeWalkerArgs encodeWalkerArgs{
        .argsExtended = &argsExtended,
        .kernelExecutionType = kernel.getExecutionType(),
        .requiredDispatchWalkOrder = kernelAttributes.dispatchWalkOrder,
        .localRegionSize = kernelAttributes.localRegionSize,
        .maxFrontEndThreads = device.getDeviceInfo().maxFrontEndThreads,
        .requiredSystemFence = kernelSystemAllocation && walkerArgs.event != nullptr,
        .hasSample = kernelInfo.kernelDescriptor.kernelAttributes.flags.hasSample};

    HardwareInterfaceHelper::setEncodeWalkerArgsExt(encodeWalkerArgs, kernelInfo);

    EncodeDispatchKernel<GfxFamily>::template encodeAdditionalWalkerFields<WalkerType>(rootDeviceEnvironment, walkerCmd, encodeWalkerArgs);
    EncodeDispatchKernel<GfxFamily>::template encodeWalkerPostSyncFields<WalkerType>(walkerCmd, rootDeviceEnvironment, encodeWalkerArgs);
    EncodeDispatchKernel<GfxFamily>::template encodeComputeDispatchAllWalker<WalkerType, InterfaceDescriptorType>(walkerCmd, interfaceDescriptor, rootDeviceEnvironment, encodeWalkerArgs);
    EncodeDispatchKernel<GfxFamily>::template overrideDefaultValues<WalkerType, InterfaceDescriptorType>(walkerCmd, *interfaceDescriptor);

    auto devices = queueCsr.getOsContext().getDeviceBitfield();
    auto partitionWalker = ImplicitScalingHelper::isImplicitScalingEnabled(devices, true);

    if (timestampPacketNode && debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
        auto gpuVa = walkerArgs.currentTimestampPacketNodes->peekNodes()[walkerArgs.currentDispatchIndex]->getGpuAddress();
        printf("\nPID:%u, TSP used for Walker: 0x%" PRIX64 ", cmdBuffer pos: 0x%" PRIX64, SysCalls::getProcessId(), gpuVa, commandStream.getCurrentGpuAddressPosition());
    }

    uint32_t workgroupSize = static_cast<uint32_t>(walkerArgs.localWorkSizes[0] * walkerArgs.localWorkSizes[1] * walkerArgs.localWorkSizes[2]);

    uint32_t maxWgCountPerTile = kernel.getMaxWorkGroupCount(dim, walkerArgs.localWorkSizes, &commandQueue, true);

    if (partitionWalker) {
        const uint64_t workPartitionAllocationGpuVa = queueCsr.getWorkPartitionAllocationGpuAddress();
        uint32_t partitionCount = 0u;
        RequiredPartitionDim requiredPartitionDim = kernel.usesImages() ? RequiredPartitionDim::x : RequiredPartitionDim::none;

        if (kernelAttributes.partitionDim != NEO::RequiredPartitionDim::none) {
            requiredPartitionDim = kernelAttributes.partitionDim;
        }

        ImplicitScalingDispatchCommandArgs implicitScalingArgs{
            workPartitionAllocationGpuVa,        // workPartitionAllocationGpuVa
            &device,                             // device
            nullptr,                             // outWalkerPtr
            requiredPartitionDim,                // requiredPartitionDim
            partitionCount,                      // partitionCount
            workgroupSize,                       // workgroupSize
            threadGroupCount,                    // threadGroupCount
            maxWgCountPerTile,                   // maxWgCountPerTile
            false,                               // useSecondaryBatchBuffer
            false,                               // apiSelfCleanup
            queueCsr.getDcFlushSupport(),        // dcFlush
            kernel.isSingleSubdevicePreferred(), // forceExecutionOnSingleTile
            false,                               // blockDispatchToCommandBuffer
            requiredWalkOrder != 0};             // isRequiredDispatchWorkGroupOrder

        ImplicitScalingDispatch<GfxFamily>::template dispatchCommands<WalkerType>(commandStream,
                                                                                  walkerCmd,
                                                                                  devices,
                                                                                  implicitScalingArgs);

        if (queueCsr.isStaticWorkPartitioningEnabled()) {
            queueCsr.setActivePartitions(std::max(queueCsr.getActivePartitions(), implicitScalingArgs.partitionCount));
        }

        if (timestampPacketNode) {
            timestampPacketNode->setPacketsUsed(implicitScalingArgs.partitionCount);
        }
    } else {
        EncodeDispatchKernel<GfxFamily>::setWalkerRegionSettings(walkerCmd, device, 1, workgroupSize, threadGroupCount, maxWgCountPerTile, requiredWalkOrder != 0);
        auto computeWalkerOnStream = commandStream.getSpaceForCmd<WalkerType>();
        *computeWalkerOnStream = walkerCmd;
    }
}
} // namespace NEO
