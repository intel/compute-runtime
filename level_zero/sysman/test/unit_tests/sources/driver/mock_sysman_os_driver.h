/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "level_zero/sysman/source/driver/sysman_os_driver.h"

namespace L0 {
namespace Sysman {
namespace ult {

// OS-agnostic mock for OsDriver. Controls what initSurvivabilityDevicesWithDriver()
// returns, allowing tests to simulate survivability-device scenarios without any
// OS-specific calls.
struct MockOsDriver : public OsDriver {
    // Set before calling MockSysmanDriver::initialize() to control survivability outcome.
    SysmanDriverHandle *survivabilityHandle = nullptr;
    ze_result_t survivabilityResult = ZE_RESULT_SUCCESS;
    uint32_t survivabilityDriverCount = 0;

    std::vector<std::unique_ptr<NEO::HwDeviceId>> discoverDevicesWithSurvivabilityMode() override {
        return {};
    }

    void initSurvivabilityDevices(_ze_driver_handle_t *, ze_result_t *result) override {
        *result = ZE_RESULT_SUCCESS;
    }

    SysmanDriverHandle *initSurvivabilityDevicesWithDriver(ze_result_t *result, uint32_t *driverCount) override {
        *result = survivabilityResult;
        *driverCount = survivabilityDriverCount;
        return survivabilityHandle;
    }
};

} // namespace ult
} // namespace Sysman
} // namespace L0
