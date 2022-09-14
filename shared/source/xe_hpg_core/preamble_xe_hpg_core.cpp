/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {
struct XeHpgCoreFamily;
using Family = XeHpgCoreFamily;
} // namespace NEO

#include "shared/source/helpers/preamble_xehp_and_later.inl"

namespace NEO {

template <>
void PreambleHelper<Family>::appendProgramVFEState(const HardwareInfo &hwInfo, const StreamProperties &streamProperties, void *cmd) {
    auto command = static_cast<typename Family::CFE_STATE *>(cmd);

    command->setComputeOverdispatchDisable(streamProperties.frontEndState.disableOverdispatch.value == 1);
    command->setSingleSliceDispatchCcsMode(streamProperties.frontEndState.singleSliceDispatchCcsMode.value == 1);

    if (DebugManager.flags.CFEComputeOverdispatchDisable.get() != -1) {
        command->setComputeOverdispatchDisable(DebugManager.flags.CFEComputeOverdispatchDisable.get());
    }
    if (DebugManager.flags.CFESingleSliceDispatchCCSMode.get() != -1) {
        command->setSingleSliceDispatchCcsMode(DebugManager.flags.CFESingleSliceDispatchCCSMode.get());
    }

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    if (!hwHelper.isFusedEuDispatchEnabled(hwInfo, streamProperties.frontEndState.disableEUFusion.value == 1)) {
        command->setFusedEuDispatch(true);
    }

    command->setNumberOfWalkers(1);
    if (DebugManager.flags.CFENumberOfWalkers.get() != -1) {
        command->setNumberOfWalkers(DebugManager.flags.CFENumberOfWalkers.get());
    }
}

template struct PreambleHelper<Family>;

} // namespace NEO
