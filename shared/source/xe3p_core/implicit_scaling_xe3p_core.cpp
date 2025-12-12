/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_container/implicit_scaling_xehp_and_later.inl"
#include "shared/source/command_container/walker_partition_from_xe3p_and_later.inl"
#include "shared/source/xe3p_core/hw_cmds_base.h"

namespace NEO {
using Family = Xe3pCoreFamily;
using COMPUTE_WALKER = Family::COMPUTE_WALKER;
using COMPUTE_WALKER_2 = Family::COMPUTE_WALKER_2;

template <>
bool ImplicitScalingDispatch<Family>::pipeControlStallRequired = false;

template <>
uint32_t ImplicitScalingDispatch<Family>::getImmediateWritePostSyncOffset() {
    return static_cast<uint32_t>(GfxCoreHelperHw<Family>::getSingleTimestampPacketSizeHw());
}

template <>
bool ImplicitScalingDispatch<Family>::platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return true;
}

template struct ImplicitScalingDispatch<Family>;
template void ImplicitScalingDispatch<Family>::dispatchCommands<COMPUTE_WALKER>(LinearStream &commandStream, COMPUTE_WALKER &walkerCmd, const DeviceBitfield &devices,
                                                                                ImplicitScalingDispatchCommandArgs &dispatchCommandArgs);
template void ImplicitScalingDispatch<Family>::dispatchCommands<COMPUTE_WALKER_2>(LinearStream &commandStream, COMPUTE_WALKER_2 &walkerCmd, const DeviceBitfield &devices,
                                                                                  ImplicitScalingDispatchCommandArgs &dispatchCommandArgs);
template size_t ImplicitScalingDispatch<Family>::getSize<COMPUTE_WALKER>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);
template size_t ImplicitScalingDispatch<Family>::getSize<COMPUTE_WALKER_2>(bool apiSelfCleanup, bool preferStaticPartitioning, const DeviceBitfield &devices, const Vec3<size_t> &groupStart, const Vec3<size_t> &groupCount);
} // namespace NEO

template void WalkerPartition::appendWalkerFields<NEO::Family, NEO::COMPUTE_WALKER>(NEO::COMPUTE_WALKER &walkerCmd, uint32_t tileCount, uint32_t workgroupCount);
template void WalkerPartition::appendWalkerFields<NEO::Family, NEO::COMPUTE_WALKER_2>(NEO::COMPUTE_WALKER_2 &walkerCmd, uint32_t tileCount, uint32_t workgroupCount);
