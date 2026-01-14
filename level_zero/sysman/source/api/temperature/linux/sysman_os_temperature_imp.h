/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/temperature/sysman_os_temperature.h"

namespace L0 {
namespace Sysman {

class LinuxSysmanImp;
class SysmanProductHelper;
struct OsSysman;

class LinuxTemperatureImp : public OsTemperature, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t getSensorTemperature(double *pTemperature) override;
    bool isTempModuleSupported() override;
    void setSensorType(zes_temp_sensors_t sensorType);
    LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, uint32_t sensorIndex);
    LinuxTemperatureImp() = default;
    ~LinuxTemperatureImp() override = default;

  protected:
    zes_temp_sensors_t type = ZES_TEMP_SENSORS_GLOBAL;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

  private:
    ze_result_t getGlobalMaxTemperature(double *pTemperature);
    ze_result_t getGpuMaxTemperature(double *pTemperature);
    ze_result_t getMemoryMaxTemperature(double *pTemperature);
    ze_result_t getVoltageRegulatorTemperature(double *pTemperature);
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    uint32_t sensorIndex = 0;
};

} // namespace Sysman
} // namespace L0
