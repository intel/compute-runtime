/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/device/sysman_hw_device_id.h"

namespace L0 {
namespace Sysman {

struct OsSysmanSurvivabilityDevice {
    static SysmanDevice *createSurvivabilityDevice(std::unique_ptr<NEO::HwDeviceId> hwDeviceId);
};

} // namespace Sysman
} // namespace L0
