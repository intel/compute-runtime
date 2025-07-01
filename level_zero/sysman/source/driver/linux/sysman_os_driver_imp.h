/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/driver/sysman_os_driver.h"

namespace L0 {
namespace Sysman {

class LinuxDriverImp : public OsDriver, NEO::NonCopyableAndNonMovableClass {
  public:
    std::vector<std::unique_ptr<NEO::HwDeviceId>> discoverDevicesWithSurvivabilityMode() override;
    void initSurvivabilityDevices(_ze_driver_handle_t *sysmanDriverHandle, ze_result_t *result) override;
    SysmanDriverHandle *initSurvivabilityDevicesWithDriver(ze_result_t *result, uint32_t *driverCount) override;
    SysmanDriverHandle *createInSurvivabilityMode(std::vector<std::unique_ptr<NEO::HwDeviceId>> &&hwDeviceId, ze_result_t *returnValue);
    LinuxDriverImp() = default;
    ~LinuxDriverImp() override = default;

  protected:
    ze_result_t initializeInSurvivabilityMode(std::vector<std::unique_ptr<NEO::HwDeviceId>> &&hwDeviceIds, SysmanDriverHandleImp *pSysmanDriverHandleImp);
};

} // namespace Sysman
} // namespace L0
