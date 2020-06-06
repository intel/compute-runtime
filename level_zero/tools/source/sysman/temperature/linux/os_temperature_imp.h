/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/temperature/os_temperature.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxTemperatureImp : public OsTemperature, public NEO::NonCopyableClass {
  public:
    ze_result_t getSensorTemperature(double *pTemperature) override;
    bool isTempModuleSupported() override;
    void setSensorType(zet_temp_sensors_t sensorType) override;
    LinuxTemperatureImp(OsSysman *pOsSysman);
    LinuxTemperatureImp() = default;
    ~LinuxTemperatureImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    zet_temp_sensors_t type;
};
} // namespace L0