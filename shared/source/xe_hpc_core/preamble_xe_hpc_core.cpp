/*
 * Copyright (C) 2020-2024 Intel Corporation
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
#include "shared/source/helpers/preamble_xe_hpg_and_xe_hpc.inl"
#include "shared/source/helpers/preamble_xehp_and_later.inl"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

using CFE_STATE = typename Family::CFE_STATE;
template <>
void PreambleHelper<Family>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {
    auto command = static_cast<CFE_STATE *>(cmd);

    if (streamProperties.frontEndState.computeDispatchAllWalkerEnable.value > 0) {
        command->setComputeDispatchAllWalkerEnable(true);
    }

    if (debugManager.flags.CFEComputeDispatchAllWalkerEnable.get() != -1) {
        command->setComputeDispatchAllWalkerEnable(debugManager.flags.CFEComputeDispatchAllWalkerEnable.get());
    }

    command->setNumberOfWalkers(1);
    if (debugManager.flags.CFENumberOfWalkers.get() != -1) {
        command->setNumberOfWalkers(debugManager.flags.CFENumberOfWalkers.get());
    }
    if (debugManager.flags.CFELargeGRFThreadAdjustDisable.get() != -1) {
        command->setLargeGRFThreadAdjustDisable(debugManager.flags.CFELargeGRFThreadAdjustDisable.get());
    }
}

template struct PreambleHelper<Family>;

} // namespace NEO
