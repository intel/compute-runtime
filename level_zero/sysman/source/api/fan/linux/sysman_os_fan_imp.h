/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/fan/sysman_os_fan.h"

namespace L0 {
namespace Sysman {

class SysfsAccess;

class LinuxFanImp : public OsFan, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_fan_properties_t *pProperties) override;
    ze_result_t getConfig(zes_fan_config_t *pConfig) override;
    ze_result_t setDefaultMode() override;
    ze_result_t setFixedSpeedMode(const zes_fan_speed_t *pSpeed) override;
    ze_result_t setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) override;
    ze_result_t getState(zes_fan_speed_units_t units, int32_t *pSpeed) override;
    bool isFanModuleSupported() override;
    LinuxFanImp(OsSysman *pOsSysman);
    LinuxFanImp() = default;
    ~LinuxFanImp() override = default;
};
} // namespace Sysman
} // namespace L0
