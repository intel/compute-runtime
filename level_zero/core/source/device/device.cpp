/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"

#include "shared/source/device/device.h"

namespace L0 {

uint32_t Device::getRootDeviceIndex() const {
    return neoDevice->getRootDeviceIndex();
}

NEO::DebuggerL0 *Device::getL0Debugger() {
    return getNEODevice()->getL0Debugger();
}

} // namespace L0