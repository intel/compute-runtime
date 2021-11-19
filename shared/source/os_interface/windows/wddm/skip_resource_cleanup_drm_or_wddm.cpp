/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

bool Wddm::skipResourceCleanup() const {
    D3DKMT_GETDEVICESTATE deviceState = {};
    deviceState.hDevice = device;
    deviceState.StateType = D3DKMT_DEVICESTATE_PRESENT;

    NTSTATUS status = STATUS_SUCCESS;
    status = getGdi()->getDeviceState(&deviceState);
    return status != STATUS_SUCCESS;
}

} // namespace NEO
