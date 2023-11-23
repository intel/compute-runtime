/*
 * Copyright (C) 2021-2023 Intel Corporation
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
template void ImplicitScalingDispatch<Family>::dispatchCommands<Family::WALKER_TYPE>(LinearStream &commandStream, Family::WALKER_TYPE &walkerCmd, void **outWalkerPtr, const DeviceBitfield &devices, uint32_t &partitionCount, bool useSecondaryBatchBuffer, bool apiSelfCleanup, bool usesImages, bool dcFlush, bool forceExecutionOnSingleTile, uint64_t workPartitionAllocationGpuVa, const HardwareInfo &hwInfo);
template size_t ImplicitScalingDispatch<Family>::getSize<Family::WALKER_TYPE>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);

} // namespace NEO
