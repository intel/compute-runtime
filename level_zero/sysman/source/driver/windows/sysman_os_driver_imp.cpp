/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/windows/sysman_os_driver_imp.h"

namespace L0 {
namespace Sysman {

std::vector<std::unique_ptr<NEO::HwDeviceId>> WddmDriverImp::discoverDevicesWithSurvivabilityMode() {
    std::vector<std::unique_ptr<NEO::HwDeviceId>> hwSurvivabilityDeviceIds;
    hwSurvivabilityDeviceIds.clear();
    return hwSurvivabilityDeviceIds;
}

void WddmDriverImp::initSurvivabilityDevices(_ze_driver_handle_t *sysmanDriverHandle, ze_result_t *result) {
}

SysmanDriverHandle *WddmDriverImp::initSurvivabilityDevicesWithDriver(ze_result_t *result, uint32_t *driverCount) {
    return nullptr;
}

std::unique_ptr<OsDriver> OsDriver::create() {
    std::unique_ptr<WddmDriverImp> pWddmDriverImp = std::make_unique<WddmDriverImp>();
    return pWddmDriverImp;
}

} // namespace Sysman
} // namespace L0
