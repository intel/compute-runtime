/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/sysman/source/device/sysman_hw_device_id.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include <level_zero/zes_api.h>

namespace L0 {
namespace Sysman {
class OsDriver {
  public:
    virtual std::vector<std::unique_ptr<NEO::HwDeviceId>> discoverDevicesWithSurvivabilityMode() = 0;
    virtual void initSurvivabilityDevices(_ze_driver_handle_t *sysmanDriverHandle, ze_result_t *result) = 0;
    virtual SysmanDriverHandle *initSurvivabilityDevicesWithDriver(ze_result_t *result, uint32_t *driverCount) = 0;
    static std::unique_ptr<OsDriver> create();
    virtual ~OsDriver() = default;
};

} // namespace Sysman
} // namespace L0
