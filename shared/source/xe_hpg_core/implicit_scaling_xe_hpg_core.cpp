/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/command_container/walker_partition_xehp_and_later.inl"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {

using Family = XeHpgCoreFamily;
using DefaultWalkerType = Family::DefaultWalkerType;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = true;

template struct ImplicitScalingDispatch<Family>;
template void ImplicitScalingDispatch<Family>::dispatchCommands<DefaultWalkerType>(LinearStream &commandStream, DefaultWalkerType &walkerCmd, const DeviceBitfield &devices,
                                                                                   ImplicitScalingDispatchCommandArgs &dispatchCommandArgs);
template size_t ImplicitScalingDispatch<Family>::getSize<DefaultWalkerType>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);
} // namespace NEO

template void WalkerPartition::appendWalkerFields<NEO::Family, NEO::DefaultWalkerType>(NEO::DefaultWalkerType &walkerCmd, uint32_t tileCount, uint32_t workgroupCount);
