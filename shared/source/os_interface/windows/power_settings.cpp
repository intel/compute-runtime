/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/sys_calls.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Wddm::setProcessPowerThrottling() {
    SysCalls::ProcessPowerThrottlingState processPowerThrottlingState{};
    bool doSetProcessPowerThrottlingState = false;
    if (debugManager.flags.SetProcessPowerThrottlingState.get() != -1) {
        switch (debugManager.flags.SetProcessPowerThrottlingState.get()) {
        case 0:
            doSetProcessPowerThrottlingState = false;
            break;
        case 1:
            doSetProcessPowerThrottlingState = true;
            processPowerThrottlingState = SysCalls::ProcessPowerThrottlingState::Eco;
            break;
        case 2:
            doSetProcessPowerThrottlingState = true;
            processPowerThrottlingState = SysCalls::ProcessPowerThrottlingState::High;
            break;
        default:
            break;
        }
    }
    if (doSetProcessPowerThrottlingState) {
        SysCalls::setProcessPowerThrottlingState(processPowerThrottlingState);
    }
}

} // namespace NEO