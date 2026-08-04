/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/source/driver/sysman_os_driver.h"

#include <memory>
#include <mutex>

namespace L0 {
namespace Sysman {

class SysmanDriverImp : public SysmanDriver {
  public:
    ze_result_t driverInit(zes_init_flags_t flags) override;

    void initialize(ze_result_t *result, zes_init_flags_t flags) override;
    using HwDeviceIds = std::vector<std::unique_ptr<NEO::HwDeviceId>>;
    static uint32_t discoverAndInitializeDevices(NEO::ExecutionEnvironment &executionEnvironment, HwDeviceIds &hwDeviceIds,
                                                 const char *errorPrefix);

  protected:
    std::once_flag initDriverOnce;
    static ze_result_t initStatus;

    virtual HwDeviceIds discoverHwDevices(NEO::ExecutionEnvironment &executionEnvironment);
    virtual SysmanDriverHandle *createDeferredHandle(NEO::ExecutionEnvironment &executionEnvironment, ze_result_t *result);
    virtual std::unique_ptr<OsDriver> createOsDriver();
};

} // namespace Sysman
} // namespace L0
