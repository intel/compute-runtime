/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/source/os_interface/windows/sys_calls.h"

using namespace NEO;

void KmdNotifyHelper::updateAcLineStatus() {
    SYSTEM_POWER_STATUS systemPowerStatus = {};
    auto powerStatusRetValue = SysCalls::getSystemPowerStatus(&systemPowerStatus);
    if (powerStatusRetValue == 1) {
        acLineConnected = (systemPowerStatus.ACLineStatus == 1);
    }
}
