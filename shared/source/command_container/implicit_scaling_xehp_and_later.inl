/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
WalkerPartition::WalkerPartitionArgs prepareWalkerPartitionArgs(uint64_t workPartitionAllocationGpuVa,
                                                                uint32_t tileCount,
                                                                uint32_t partitionCount,
                                                                bool emitSelfCleanup,
                                                                bool preferStaticPartitioning,
                                                                bool staticPartitioning,
                                                                bool useSecondaryBatchBuffer) {
    WalkerPartition::WalkerPartitionArgs args = {};

    args.workPartitionAllocationGpuVa = workPartitionAllocationGpuVa;
    args.partitionCount = partitionCount;
    args.tileCount = tileCount;
    args.staticPartitioning = staticPartitioning;
    args.preferredStaticPartitioning = preferStaticPartitioning;

    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.initializeWparidRegister = ImplicitScalingHelper::isWparidRegisterInitializationRequired();

    args.emitPipeControlStall = ImplicitScalingHelper::isPipeControlStallRequired(ImplicitScalingDispatch<GfxFamily>::getPipeControlStallRequired());

    args.synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    args.crossTileAtomicSynchronization = ImplicitScalingHelper::isCrossTileAtomicRequired(args.emitPipeControlStall);
    args.semaphoreProgrammingRequired = ImplicitScalingHelper::isSemaphoreProgrammingRequired();

    args.emitSelfCleanup = ImplicitScalingHelper::isSelfCleanupRequired(args, emitSelfCleanup);
    args.emitBatchBufferEnd = false;
    args.secondaryBatchBuffer = useSecondaryBatchBuffer;

    return args;
}

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getSize(bool apiSelfCleanup,
                                                   bool preferStaticPartitioning,
                                                   const DeviceBitfield &devices,
                                                   const Vec3<size_t> &groupStart,
                                                   const Vec3<size_t> &groupCount) {
    typename GfxFamily::COMPUTE_WALKER::PARTITION_TYPE partitionType{};
    bool staticPartitioning = false;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());

    const uint32_t partitionCount = WalkerPartition::computePartitionCountAndPartitionType<GfxFamily>(tileCount,
                                                                                                      preferStaticPartitioning,
                                                                                                      groupStart,
                                                                                                      groupCount,
                                                                                                      {},
                                                                                                      &partitionType,
                                                                                                      &staticPartitioning);
    UNRECOVERABLE_IF(staticPartitioning && (tileCount != partitionCount));
    WalkerPartition::WalkerPartitionArgs args = prepareWalkerPartitionArgs<GfxFamily>(0u,
                                                                                      tileCount,
                                                                                      partitionCount,
                                                                                      apiSelfCleanup,
                                                                                      preferStaticPartitioning,
                                                                                      staticPartitioning,
                                                                                      false);

    return static_cast<size_t>(WalkerPartition::estimateSpaceRequiredInCommandBuffer<GfxFamily>(args));
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchCommands(LinearStream &commandStream,
                                                          WALKER_TYPE &walkerCmd,
                                                          const DeviceBitfield &devices,
                                                          uint32_t &partitionCount,
                                                          bool useSecondaryBatchBuffer,
                                                          bool apiSelfCleanup,
                                                          bool usesImages,
                                                          uint64_t workPartitionAllocationGpuVa,
                                                          const HardwareInfo &hwInfo) {
    uint32_t totalProgrammedSize = 0u;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());
    const bool preferStaticPartitioning = workPartitionAllocationGpuVa != 0u;

    bool staticPartitioning = false;
    partitionCount = WalkerPartition::computePartitionCountAndSetPartitionType<GfxFamily>(&walkerCmd, tileCount, preferStaticPartitioning, usesImages, &staticPartitioning);

    WalkerPartition::WalkerPartitionArgs args = prepareWalkerPartitionArgs<GfxFamily>(workPartitionAllocationGpuVa,
                                                                                      tileCount,
                                                                                      partitionCount,
                                                                                      apiSelfCleanup,
                                                                                      preferStaticPartitioning,
                                                                                      staticPartitioning,
                                                                                      useSecondaryBatchBuffer);

    auto dispatchCommandsSize = getSize(apiSelfCleanup, preferStaticPartitioning, devices, {walkerCmd.getThreadGroupIdStartingX(), walkerCmd.getThreadGroupIdStartingY(), walkerCmd.getThreadGroupIdStartingZ()}, {walkerCmd.getThreadGroupIdXDimension(), walkerCmd.getThreadGroupIdYDimension(), walkerCmd.getThreadGroupIdZDimension()});
    void *commandBuffer = commandStream.getSpace(dispatchCommandsSize);
    uint64_t cmdBufferGpuAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed() - dispatchCommandsSize;

    if (staticPartitioning) {
        UNRECOVERABLE_IF(tileCount != partitionCount);
        WalkerPartition::constructStaticallyPartitionedCommandBuffer<GfxFamily>(commandBuffer,
                                                                                cmdBufferGpuAddress,
                                                                                &walkerCmd,
                                                                                totalProgrammedSize,
                                                                                args,
                                                                                hwInfo);
    } else {
        if (DebugManager.flags.ExperimentalSetWalkerPartitionCount.get()) {
            partitionCount = DebugManager.flags.ExperimentalSetWalkerPartitionCount.get();
            if (partitionCount == 1u) {
                walkerCmd.setPartitionType(GfxFamily::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
            }
            args.partitionCount = partitionCount;
        }

        WalkerPartition::constructDynamicallyPartitionedCommandBuffer<GfxFamily>(commandBuffer,
                                                                                 cmdBufferGpuAddress,
                                                                                 &walkerCmd,
                                                                                 totalProgrammedSize,
                                                                                 args,
                                                                                 hwInfo);
    }
    UNRECOVERABLE_IF(totalProgrammedSize != dispatchCommandsSize);
}

template <typename GfxFamily>
bool &ImplicitScalingDispatch<GfxFamily>::getPipeControlStallRequired() {
    return ImplicitScalingDispatch<GfxFamily>::pipeControlStallRequired;
}

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getBarrierSize(const HardwareInfo &hwInfo,
                                                          bool apiSelfCleanup,
                                                          bool usePostSync) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.emitSelfCleanup = apiSelfCleanup;
    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.usePostSync = usePostSync;

    return static_cast<size_t>(WalkerPartition::estimateBarrierSpaceRequiredInCommandBuffer<GfxFamily>(args, hwInfo));
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(LinearStream &commandStream,
                                                                 const DeviceBitfield &devices,
                                                                 PipeControlArgs &flushArgs,
                                                                 const HardwareInfo &hwInfo,
                                                                 uint64_t gpuAddress,
                                                                 uint64_t immediateData,
                                                                 bool apiSelfCleanup,
                                                                 bool useSecondaryBatchBuffer) {
    uint32_t totalProgrammedSize = 0u;

    WalkerPartition::WalkerPartitionArgs args = {};
    args.emitSelfCleanup = apiSelfCleanup;
    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.tileCount = static_cast<uint32_t>(devices.count());
    args.secondaryBatchBuffer = useSecondaryBatchBuffer;
    args.usePostSync = gpuAddress > 0;
    args.postSyncGpuAddress = gpuAddress;
    args.postSyncImmediateValue = immediateData;

    auto barrierCommandsSize = getBarrierSize(hwInfo, apiSelfCleanup, args.usePostSync);
    void *commandBuffer = commandStream.getSpace(barrierCommandsSize);
    uint64_t cmdBufferGpuAddress = commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed() - barrierCommandsSize;

    WalkerPartition::constructBarrierCommandBuffer<GfxFamily>(commandBuffer,
                                                              cmdBufferGpuAddress,
                                                              totalProgrammedSize,
                                                              args,
                                                              flushArgs,
                                                              hwInfo);
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
                                                                              uint32_t addressOffset) {
    EncodeSetMMIO<GfxFamily>::encodeMEM(commandStream,
                                        PartitionRegisters<GfxFamily>::wparidCCSOffset,
                                        workPartitionSurfaceAddress);
    dispatchOffsetRegister(commandStream, addressOffset);
}

template <typename GfxFamily>
inline size_t ImplicitScalingDispatch<GfxFamily>::getOffsetRegisterSize() {
    return EncodeSetMMIO<GfxFamily>::sizeIMM;
}

template <typename GfxFamily>
inline void ImplicitScalingDispatch<GfxFamily>::dispatchOffsetRegister(LinearStream &commandStream,
                                                                       uint32_t addressOffset) {
    EncodeSetMMIO<GfxFamily>::encodeIMM(commandStream,
                                        PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                        addressOffset,
                                        true);
}

template <typename GfxFamily>
inline uint32_t ImplicitScalingDispatch<GfxFamily>::getPostSyncOffset() {
    return static_cast<uint32_t>(HwHelperHw<GfxFamily>::getSingleTimestampPacketSizeHw());
}

template <typename GfxFamily>
inline bool ImplicitScalingDispatch<GfxFamily>::platformSupportsImplicitScaling(const HardwareInfo &hwInfo) {
    return false;
}

} // namespace NEO
