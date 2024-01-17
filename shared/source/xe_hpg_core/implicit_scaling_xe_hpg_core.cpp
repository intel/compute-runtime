/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

using Family = XeHpgCoreFamily;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = true;

template struct ImplicitScalingDispatch<Family>;
template void ImplicitScalingDispatch<Family>::dispatchCommands<Family::DefaultWalkerType>(LinearStream &commandStream, Family::DefaultWalkerType &walkerCmd, void **outWalkerPtr, const DeviceBitfield &devices, NEO::RequiredPartitionDim requiredPartitionDim, uint32_t &partitionCount, bool useSecondaryBatchBuffer, bool apiSelfCleanup, bool dcFlush, bool forceExecutionOnSingleTile, uint64_t workPartitionAllocationGpuVa, const HardwareInfo &hwInfo);
template size_t ImplicitScalingDispatch<Family>::getSize<Family::DefaultWalkerType>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);
template void ImplicitScalingDispatch<Family>::appendWalkerFields<Family::DefaultWalkerType>(Family::DefaultWalkerType &walkerCmd, uint32_t tileCount);

} // namespace NEO
