/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/temperature/sysman_os_temperature.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"

#include "igfxfmid.h"

#include <memory>

namespace L0 {
namespace Sysman {

class SysfsAccess;
class PlatformMonitoringTech;
class SysmanProductHelper;
class LinuxTemperatureImp : public OsTemperature, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t getSensorTemperature(double *pTemperature) override;
    bool isTempModuleSupported() override;
    void setSensorType(zes_temp_sensors_t sensorType);
    LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxTemperatureImp() = default;
    ~LinuxTemperatureImp() override = default;

  protected:
    PlatformMonitoringTech *pPmt = nullptr;
    zes_temp_sensors_t type = ZES_TEMP_SENSORS_GLOBAL;

  private:
    ze_result_t getGlobalMaxTemperature(double *pTemperature);
    ze_result_t getGpuMaxTemperature(double *pTemperature);
    ze_result_t getMemoryMaxTemperature(double *pTemperature);
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
};

} // namespace Sysman
} // namespace L0
