/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

namespace NEO {
struct XeHpgCoreFamily;
using Family = XeHpgCoreFamily;
} // namespace NEO

#include "shared/source/helpers/preamble_xe_hpg_and_xe_hpc.inl"
#include "shared/source/helpers/preamble_xehp_and_later.inl"

namespace NEO {

template <>
void PreambleHelper<Family>::appendProgramVFEState(const RootDeviceEnvironment &rootDeviceEnvironment, const StreamProperties &streamProperties, void *cmd) {
    auto command = static_cast<typename Family::CFE_STATE *>(cmd);

    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (!gfxCoreHelper.isFusedEuDispatchEnabled(hwInfo, streamProperties.frontEndState.disableEUFusion.value == 1)) {
        command->setFusedEuDispatch(true);
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
