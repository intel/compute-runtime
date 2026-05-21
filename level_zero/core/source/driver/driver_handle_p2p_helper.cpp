/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/driver/driver_handle.h"

namespace L0 {

bool DriverHandle::peerRequiresReservedHandleData(Device *srcDevice, Device *peerDevice) {
    return false;
}

} // namespace L0
