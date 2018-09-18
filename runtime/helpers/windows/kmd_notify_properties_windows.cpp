/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/kmd_notify_properties.h"
#include "runtime/os_interface/windows/sys_calls.h"

using namespace OCLRT;

void KmdNotifyHelper::updateAcLineStatus() {
    SYSTEM_POWER_STATUS systemPowerStatus = {};
    auto powerStatusRetValue = SysCalls::getSystemPowerStatus(&systemPowerStatus);
    if (powerStatusRetValue == 1) {
        acLineConnected = (systemPowerStatus.ACLineStatus == 1);
    }
}

int64_t KmdNotifyHelper::getBaseTimeout(const int64_t &multiplier) const {
    return properties->delayKmdNotifyMicroseconds;
}
