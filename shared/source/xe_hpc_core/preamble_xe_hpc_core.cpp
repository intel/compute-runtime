/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

namespace NEO {
struct XeHpcCoreFamily;
using Family = XeHpcCoreFamily;
} // namespace NEO

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/preamble_xehp_and_later.inl"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {

using CFE_STATE = typename Family::CFE_STATE;
template <>
void PreambleHelper<Family>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {
    auto command = static_cast<CFE_STATE *>(cmd);

    command->setComputeOverdispatchDisable(streamProperties.frontEndState.disableOverdispatch.value == 1);
    command->setSingleSliceDispatchCcsMode(streamProperties.frontEndState.singleSliceDispatchCcsMode.value == 1);

    if (streamProperties.frontEndState.computeDispatchAllWalkerEnable.value > 0) {
        command->setComputeDispatchAllWalkerEnable(true);
    }

    if (DebugManager.flags.CFEComputeDispatchAllWalkerEnable.get() != -1) {
        command->setComputeDispatchAllWalkerEnable(DebugManager.flags.CFEComputeDispatchAllWalkerEnable.get());
    }

    if (DebugManager.flags.CFEComputeOverdispatchDisable.get() != -1) {
        command->setComputeOverdispatchDisable(DebugManager.flags.CFEComputeOverdispatchDisable.get());
    }
    if (DebugManager.flags.CFESingleSliceDispatchCCSMode.get() != -1) {
        command->setSingleSliceDispatchCcsMode(DebugManager.flags.CFESingleSliceDispatchCCSMode.get());
    }

    command->setNumberOfWalkers(1);
    if (DebugManager.flags.CFENumberOfWalkers.get() != -1) {
        command->setNumberOfWalkers(DebugManager.flags.CFENumberOfWalkers.get());
    }
}

template struct PreambleHelper<Family>;

} // namespace NEO
