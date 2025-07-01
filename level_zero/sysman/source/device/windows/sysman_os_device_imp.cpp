/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/device/sysman_os_device.h"

namespace L0 {
namespace Sysman {

SysmanDevice *OsSysmanSurvivabilityDevice::createSurvivabilityDevice(std::unique_ptr<NEO::HwDeviceId> hwDeviceId) {
    return nullptr;
}

} // namespace Sysman
} // namespace L0
