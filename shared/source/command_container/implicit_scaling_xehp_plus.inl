/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/walker_partition_xehp_plus.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {

template <typename GfxFamily>
size_t ImplicitScalingDispatch<GfxFamily>::getSize(bool nativeCrossTileAtomicSync,
                                                   bool preferStaticPartitioning,
                                                   const DeviceBitfield &devices,
                                                   Vec3<size_t> groupStart,
                                                   Vec3<size_t> groupCount) {
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

    auto synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    const bool useAtomicsForNativeCleanup = ImplicitScalingHelper::useAtomicsForNativeCleanup();
    return static_cast<size_t>(WalkerPartition::estimateSpaceRequiredInCommandBuffer<GfxFamily>(false,
                                                                                                16u,
                                                                                                synchronizeBeforeExecution,
                                                                                                nativeCrossTileAtomicSync,
                                                                                                staticPartitioning,
                                                                                                useAtomicsForNativeCleanup));
}

template <typename GfxFamily>
void ImplicitScalingDispatch<GfxFamily>::dispatchCommands(LinearStream &commandStream,
                                                          WALKER_TYPE &walkerCmd,
                                                          const DeviceBitfield &devices,
                                                          uint32_t &partitionCount,
                                                          bool useSecondaryBatchBuffer,
                                                          bool nativeCrossTileAtomicSync,
                                                          bool usesImages,
                                                          uint64_t workPartitionAllocationGpuVa) {
    uint32_t totalProgrammedSize = 0u;
    const uint32_t tileCount = static_cast<uint32_t>(devices.count());
    const bool preferStaticPartitioning = workPartitionAllocationGpuVa != 0u;

    bool staticPartitioning = false;
    partitionCount = WalkerPartition::computePartitionCountAndSetPartitionType<GfxFamily>(&walkerCmd, tileCount, preferStaticPartitioning, usesImages, &staticPartitioning);
    const bool synchronizeBeforeExecution = ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired();
    const bool useAtomicsForNativeCleanup = ImplicitScalingHelper::useAtomicsForNativeCleanup();
    if (staticPartitioning) {
        UNRECOVERABLE_IF(tileCount != partitionCount);
        WalkerPartition::constructStaticallyPartitionedCommandBuffer<GfxFamily>(commandStream.getSpace(0u),
                                                                                commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed(),
                                                                                &walkerCmd,
                                                                                totalProgrammedSize,
                                                                                partitionCount,
                                                                                tileCount,
                                                                                synchronizeBeforeExecution,
                                                                                useSecondaryBatchBuffer,
                                                                                nativeCrossTileAtomicSync,
                                                                                workPartitionAllocationGpuVa,
                                                                                useAtomicsForNativeCleanup);
    } else {
        if (DebugManager.flags.ExperimentalSetWalkerPartitionCount.get()) {
            partitionCount = DebugManager.flags.ExperimentalSetWalkerPartitionCount.get();
            if (partitionCount == 1u) {
                walkerCmd.setPartitionType(GfxFamily::COMPUTE_WALKER::PARTITION_TYPE::PARTITION_TYPE_DISABLED);
            }
        }

        WalkerPartition::constructDynamicallyPartitionedCommandBuffer<GfxFamily>(commandStream.getSpace(0u),
                                                                                 commandStream.getGraphicsAllocation()->getGpuAddress() + commandStream.getUsed(),
                                                                                 &walkerCmd, totalProgrammedSize,
                                                                                 partitionCount, tileCount,
                                                                                 false, synchronizeBeforeExecution, useSecondaryBatchBuffer,
                                                                                 nativeCrossTileAtomicSync,
                                                                                 useAtomicsForNativeCleanup);
    }
    commandStream.getSpace(totalProgrammedSize);
}

} // namespace NEO
