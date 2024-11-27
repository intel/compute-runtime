/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
WalkerPartition::WalkerPartitionArgs prepareWalkerPartitionArgs(ImplicitScalingDispatchCommandArgs &dispatchCommandArgs,
                                                                uint32_t tileCount,
                                                                bool preferStaticPartitioning,
                                                                bool staticPartitioning) {
    WalkerPartition::WalkerPartitionArgs args = {};

    args.workPartitionAllocationGpuVa = dispatchCommandArgs.workPartitionAllocationGpuVa;
    args.partitionCount = dispatchCommandArgs.partitionCount;
    args.tileCount = tileCount;
    args.staticPartitioning = staticPartitioning;
    args.preferredStaticPartitioning = preferStaticPartitioning;
    args.forceExecutionOnSingleTile = dispatchCommandArgs.forceExecutionOnSingleTile;

    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.initializeWparidRegister = ImplicitScalingHelper::isWparidRegisterInitializationRequired();

    args.emitPipeControlStall = ImplicitScalingHelper::isPipeControlStallRequired(ImplicitScalingDispatch<GfxFamily>::getPipeControlStallRequired());

    args.synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    args.crossTileAtomicSynchronization = ImplicitScalingHelper::isCrossTileAtomicRequired(args.emitPipeControlStall);
    args.semaphoreProgrammingRequired = ImplicitScalingHelper::isSemaphoreProgrammingRequired();

    args.emitSelfCleanup = ImplicitScalingHelper::isSelfCleanupRequired(args, dispatchCommandArgs.apiSelfCleanup);
    args.emitBatchBufferEnd = false;
    args.secondaryBatchBuffer = dispatchCommandArgs.useSecondaryBatchBuffer;

    args.dcFlushEnable = dispatchCommandArgs.dcFlush;

    args.pipeControlBeforeCleanupCrossTileSync = ImplicitScalingHelper::pipeControlBeforeCleanupAtomicSyncRequired();

    args.blockDispatchToCommandBuffer = dispatchCommandArgs.blockDispatchToCommandBuffer;

    args.workgroupSize = dispatchCommandArgs.workgroupSize;
    args.maxWgCountPerTile = dispatchCommandArgs.maxWgCountPerTile;
    args.isRequiredDispatchWorkGroupOrder = dispatchCommandArgs.isRequiredDispatchWorkGroupOrder;

    return args;
}

template <typename GfxFamily>
template <typename WalkerType>
size_t ImplicitScalingDispatch<GfxFamily>::getSize(bool apiSelfCleanup,
                                                   bool preferStaticPartitioning,
                                                   const DeviceBitfield &devices,
                                                   const Vec3<size_t> &groupStart,
                                                   const Vec3<size_t> &groupCount) {
    typename WalkerType::PARTITION_TYPE partitionType{};
    bool staticPartitioning = false;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());

    const uint32_t partitionCount = WalkerPartition::computePartitionCountAndPartitionType<GfxFamily, WalkerType>(tileCount,
                                                                                                                  preferStaticPartitioning,
                                                                                                                  groupStart,
                                                                                                                  groupCount,
                                                                                                                  {},
                                                                                                                  &partitionType,
                                                                                                                  &staticPartitioning);
    UNRECOVERABLE_IF(staticPartitioning && (tileCount != partitionCount));
    ImplicitScalingDispatchCommandArgs dispatchCommandArgs = {};
    dispatchCommandArgs.partitionCount = partitionCount;
    dispatchCommandArgs.apiSelfCleanup = apiSelfCleanup;

    WalkerPartition::WalkerPartitionArgs args = prepareWalkerPartitionArgs<GfxFamily>(dispatchCommandArgs,
                                                                                      tileCount,
                                                                                      preferStaticPartitioning,
                                                                                      staticPartitioning);

    return static_cast<size_t>(WalkerPartition::estimateSpaceRequiredInCommandBuffer<GfxFamily, WalkerType>(args));
}

template <typename GfxFamily>
template <typename WalkerType>
void ImplicitScalingDispatch<GfxFamily>::dispatchCommands(LinearStream &commandStream,
                                                          WalkerType &walkerCmd,
                                                          const DeviceBitfield &devices,
                                                          ImplicitScalingDispatchCommandArgs &dispatchCommandArgs) {
    uint32_t totalProgrammedSize = 0u;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());
    const bool preferStaticPartitioning = dispatchCommandArgs.workPartitionAllocationGpuVa != 0u;

    bool staticPartitioning = false;
    dispatchCommandArgs.partitionCount = WalkerPartition::computePartitionCountAndSetPartitionType<GfxFamily, WalkerType>(&walkerCmd, dispatchCommandArgs.requiredPartitionDim, tileCount, preferStaticPartitioning, &staticPartitioning);

    WalkerPartition::WalkerPartitionArgs walkerPartitionArgs = prepareWalkerPartitionArgs<GfxFamily>(dispatchCommandArgs,
                                                                                                     tileCount,
                                                                                                     preferStaticPartitioning,
                                                                                                     staticPartitioning);
    size_t dispatchCommandsSize = 0;
    void *commandBuffer = nullptr;
    uint64_t cmdBufferGpuAddress = 0;

    if (!dispatchCommandArgs.blockDispatchToCommandBuffer) {
        dispatchCommandsSize = getSize<WalkerType>(dispatchCommandArgs.apiSelfCleanup,
                                                   preferStaticPartitioning,
                                                   devices,
                                                   {walkerCmd.getThreadGroupIdStartingX(), walkerCmd.getThreadGroupIdStartingY(), walkerCmd.getThreadGroupIdStartingZ()},
                                                   {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()});
        commandBuffer = commandStream.getSpace(dispatchCommandsSize);
        cmdBufferGpuAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed() - dispatchCommandsSize;
    }

    if (staticPartitioning) {
        UNRECOVERABLE_IF(tileCount != dispatchCommandArgs.partitionCount);
        WalkerPartition::constructStaticallyPartitionedCommandBuffer<GfxFamily, WalkerType>(commandBuffer,
                                                                                            dispatchCommandArgs.outWalkerPtr,
                                                                                            cmdBufferGpuAddress,
                                                                                            &walkerCmd,
                                                                                            totalProgrammedSize,
                                                                                            walkerPartitionArgs,
                                                                                            *dispatchCommandArgs.device);
    } else {
        if (debugManager.flags.ExperimentalSetWalkerPartitionCount.get()) {
            dispatchCommandArgs.partitionCount = debugManager.flags.ExperimentalSetWalkerPartitionCount.get();
            if (dispatchCommandArgs.partitionCount == 1u) {
                walkerCmd.setPartitionType(WalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
            }
            walkerPartitionArgs.partitionCount = dispatchCommandArgs.partitionCount;
        }

        WalkerPartition::constructDynamicallyPartitionedCommandBuffer<GfxFamily, WalkerType>(commandBuffer,
                                                                                             dispatchCommandArgs.outWalkerPtr,
                                                                                             cmdBufferGpuAddress,
                                                                                             &walkerCmd,
                                                                                             totalProgrammedSize,
                                                                                             walkerPartitionArgs,
                                                                                             *dispatchCommandArgs.device);
    }
    UNRECOVERABLE_IF(totalProgrammedSize != dispatchCommandsSize);
}

template <typename GfxFamily>
bool &ImplicitScalingDispatch<GfxFamily>::getPipeControlStallRequired() {
    return ImplicitScalingDispatch<GfxFamily>::pipeControlStallRequired;
}

template <typename GfxFamily>
WalkerPartition::WalkerPartitionArgs prepareBarrierWalkerPartitionArgs(bool emitSelfCleanup,
                                                                       bool usePostSync) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = true;
    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.usePostSync = usePostSync;

    args.emitSelfCleanup = ImplicitScalingHelper::isSelfCleanupRequired(args, emitSelfCleanup);
    args.pipeControlBeforeCleanupCrossTileSync = ImplicitScalingHelper::pipeControlBeforeCleanupAtomicSyncRequired();

    return args;
}

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getBarrierSize(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                          bool apiSelfCleanup,
                                                          bool usePostSync) {
    WalkerPartition::WalkerPartitionArgs args = prepareBarrierWalkerPartitionArgs<GfxFamily>(apiSelfCleanup, usePostSync);

    return static_cast<size_t>(WalkerPartition::estimateBarrierSpaceRequiredInCommandBuffer<GfxFamily>(args, rootDeviceEnvironment));
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(LinearStream &commandStream,
                                                                 const DeviceBitfield &devices,
                                                                 PipeControlArgs &flushArgs,
                                                                 const RootDeviceEnvironment &rootDeviceEnvironment,
                                                                 uint64_t gpuAddress,
                                                                 uint64_t immediateData,
                                                                 bool apiSelfCleanup,
                                                                 bool useSecondaryBatchBuffer) {
    uint32_t totalProgrammedSize = 0u;

    WalkerPartition::WalkerPartitionArgs args = prepareBarrierWalkerPartitionArgs<GfxFamily>(apiSelfCleanup, gpuAddress > 0);
    args.tileCount = static_cast<uint32_t>(devices.count());
    args.secondaryBatchBuffer = useSecondaryBatchBuffer;
    args.postSyncGpuAddress = gpuAddress;
    args.postSyncImmediateValue = immediateData;

    auto barrierCommandsSize = getBarrierSize(rootDeviceEnvironment, args.emitSelfCleanup, args.usePostSync);
    void *commandBuffer = commandStream.getSpace(barrierCommandsSize);
    uint64_t cmdBufferGpuAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed() - barrierCommandsSize;

    WalkerPartition::constructBarrierCommandBuffer<GfxFamily>(commandBuffer,
                                                              cmdBufferGpuAddress,
                                                              totalProgrammedSize,
                                                              args,
                                                              flushArgs,
                                                              rootDeviceEnvironment);
    UNRECOVERABLE_IF(totalProgrammedSize != barrierCommandsSize);
}

template <typename GfxFamily>
inline size_t ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize() {
    return EncodeSetMMIO<GfxFamily>::sizeMEM +
           getOffsetRegisterSize();
}

template <typename GfxFamily>
inline void ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(LinearStream &commandStream,
                                                                              uint64_t workPartitionSurfaceAddress,
                                                                              uint32_t addressOffset,
                                                                              bool isBcs) {
    EncodeSetMMIO<GfxFamily>::encodeMEM(commandStream,
                                        PartitionRegisters<GfxFamily>::wparidCCSOffset,
                                        workPartitionSurfaceAddress, isBcs);
    dispatchOffsetRegister(commandStream, addressOffset, isBcs);
}

template <typename GfxFamily>
inline size_t ImplicitScalingDispatch<GfxFamily>::getOffsetRegisterSize() {
    return EncodeSetMMIO<GfxFamily>::sizeIMM;
}

template <typename GfxFamily>
inline void ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(LinearStream &commandStream,
                                                                       uint32_t addressOffset, bool isBcs) {
    EncodeSetMMIO<GfxFamily>::encodeIMM(commandStream,
                                        PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                        addressOffset,
                                        true,
                                        isBcs);
}

template <typename GfxFamily>
inline uint32_t ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset() {
    return static_cast<uint32_t>(sizeof(uint64_t));
}

template <typename GfxFamily>
inline uint32_t ImplicitScalingDispatch<GfxFamily>::getTimeStampPostSyncOffset() {
    return static_cast<uint32_t>(GfxCoreHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw());
}

template <typename GfxFamily>
inline bool ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return false;
}

} // namespace NEO
