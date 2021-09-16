/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getSize(bool emitSelfCleanup,
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
    WalkerPartition::WalkerPartitionArgs args = {};

    args.partitionCount = partitionCount;
    args.tileCount = tileCount;
    args.synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.emitSelfCleanup = ImplicitScalingHelper::isSelfCleanupRequired(emitSelfCleanup);
    args.initializeWparidRegister = ImplicitScalingHelper::isWparidRegisterInitializationRequired();
    args.crossTileAtomicSynchronization = ImplicitScalingHelper::isCrossTileAtomicRequired();
    args.semaphoreProgrammingRequired = ImplicitScalingHelper::isSemaphoreProgrammingRequired();
    args.emitPipeControlStall = ImplicitScalingHelper::isPipeControlStallRequired();
    args.emitBatchBufferEnd = false;
    args.staticPartitioning = staticPartitioning;

    return static_cast<size_t>(WalkerPartition::estimateSpaceRequiredInCommandBuffer<GfxFamily>(args));
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchCommands(LinearStream &commandStream,
                                                          WALKER_TYPE &walkerCmd,
                                                          const DeviceBitfield &devices,
                                                          uint32_t &partitionCount,
                                                          bool useSecondaryBatchBuffer,
                                                          bool emitSelfCleanup,
                                                          bool usesImages,
                                                          uint64_t workPartitionAllocationGpuVa) {
    uint32_t totalProgrammedSize = 0u;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());
    const bool preferStaticPartitioning = workPartitionAllocationGpuVa != 0u;

    bool staticPartitioning = false;
    partitionCount = WalkerPartition::computePartitionCountAndSetPartitionType<GfxFamily>(&walkerCmd, tileCount, preferStaticPartitioning, usesImages, &staticPartitioning);

    WalkerPartition::WalkerPartitionArgs args = {};
    args.workPartitionAllocationGpuVa = workPartitionAllocationGpuVa;
    args.partitionCount = partitionCount;
    args.tileCount = tileCount;
    args.synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    args.useAtomicsForSelfCleanup = ImplicitScalingHelper::isAtomicsUsedForSelfCleanup();
    args.emitSelfCleanup = ImplicitScalingHelper::isSelfCleanupRequired(emitSelfCleanup);
    args.initializeWparidRegister = ImplicitScalingHelper::isWparidRegisterInitializationRequired();
    args.crossTileAtomicSynchronization = ImplicitScalingHelper::isCrossTileAtomicRequired();
    args.semaphoreProgrammingRequired = ImplicitScalingHelper::isSemaphoreProgrammingRequired();
    args.emitPipeControlStall = ImplicitScalingHelper::isPipeControlStallRequired();
    args.emitBatchBufferEnd = false;
    args.secondaryBatchBuffer = useSecondaryBatchBuffer;
    args.staticPartitioning = staticPartitioning;

    if (staticPartitioning) {
        UNRECOVERABLE_IF(tileCount != partitionCount);
        WalkerPartition::constructStaticallyPartitionedCommandBuffer<GfxFamily>(commandStream.getSpace(0u),
                                                                                commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed(),
                                                                                &walkerCmd,
                                                                                totalProgrammedSize,
                                                                                args);
    } else {
        if (DebugManager.flags.ExperimentalSetWalkerPartitionCount.get()) {
            partitionCount = DebugManager.flags.ExperimentalSetWalkerPartitionCount.get();
            if (partitionCount == 1u) {
                walkerCmd.setPartitionType(GfxFamily::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
            }
            args.partitionCount = partitionCount;
        }

        WalkerPartition::constructDynamicallyPartitionedCommandBuffer<GfxFamily>(commandStream.getSpace(0u),
                                                                                 commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed(),
                                                                                 &walkerCmd,
                                                                                 totalProgrammedSize,
                                                                                 args);
    }
    commandStream.getSpace(totalProgrammedSize);
}

} // namespace NEO
