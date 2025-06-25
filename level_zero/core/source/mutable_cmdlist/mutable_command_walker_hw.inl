/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.h"

namespace L0::MCL {

template <typename GfxFamily>
template <typename WalkerType>
void MutableComputeWalkerHw<GfxFamily>::updateImplicitScalingData(const NEO::Device &device,
                                                                  uint32_t partitionCount,
                                                                  uint32_t workgroupSize,
                                                                  uint32_t threadGroupCount,
                                                                  uint32_t maxWgCountPerTile,
                                                                  NEO::RequiredPartitionDim requiredPartitionDim,
                                                                  bool isRequiredDispatchWorkGroupOrder,
                                                                  bool cooperativeKernel) {

    constexpr uint64_t viewOnlyStaticPartitionGpuVa = 64u;
    constexpr bool dcFlushEnable = false;
    constexpr bool onlyRegularCmdList = true;
    constexpr bool useSecondaryBatchBuffer = false;
    constexpr bool makeCommandView = true;

    bool forceExecutionOnSingleTile = NEO::EncodeDispatchKernel<GfxFamily>::singleTileExecImplicitScalingRequired(cooperativeKernel);

    NEO::ImplicitScalingDispatchCommandArgs implicitScalingArgs{
        viewOnlyStaticPartitionGpuVa,      // workPartitionAllocationGpuVa
        &device,                           // device
        nullptr,                           // outWalkerPtr
        requiredPartitionDim,              // requiredPartitionDim
        partitionCount,                    // partitionCount
        workgroupSize,                     // workgroupSize
        threadGroupCount,                  // threadGroupCount
        maxWgCountPerTile,                 // maxWgCountPerTile
        useSecondaryBatchBuffer,           // useSecondaryBatchBuffer
        onlyRegularCmdList,                // apiSelfCleanup
        dcFlushEnable,                     // dcFlush
        forceExecutionOnSingleTile,        // forceExecutionOnSingleTile
        makeCommandView,                   // blockDispatchToCommandBuffer
        isRequiredDispatchWorkGroupOrder}; // isRequiredDispatchWorkGroupOrder

    NEO::LinearStream viewOnlyStream;

    auto walkerCpuCmd = reinterpret_cast<WalkerType *>(this->cpuBuffer);

    NEO::ImplicitScalingDispatch<GfxFamily>::dispatchCommands(viewOnlyStream,
                                                              *walkerCpuCmd,
                                                              device.getDeviceBitfield(),
                                                              implicitScalingArgs);
}

template <typename GfxFamily>
void MutableComputeWalkerHw<GfxFamily>::updateSlmSize(const NEO::Device &device, uint32_t slmTotalSize) {
    auto slmSize = NEO::EncodeDispatchKernel<GfxFamily>::computeSlmValues(device.getHardwareInfo(), slmTotalSize, device.getReleaseHelper(), this->isHeapless);

    if (NEO::debugManager.flags.OverrideSlmAllocationSize.get() != -1) {
        slmSize = static_cast<uint32_t>(NEO::debugManager.flags.OverrideSlmAllocationSize.get());
    }

    this->setSlmSize(slmSize);
}

} // namespace L0::MCL
