/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/device/sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {

std::unique_ptr<NEO::HwDeviceId> createSysmanHwDeviceId(std::unique_ptr<NEO::HwDeviceId> &hwDeviceId) {
    return std::move(hwDeviceId);
}

} // namespace Sysman
} // namespace L0