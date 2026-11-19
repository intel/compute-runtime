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

class FsAccessInterface;
class LinuxSysmanImp;
class SysmanProductHelper;
struct OsSysman;
enum class NodeName;

class LinuxTemperatureImp : public OsTemperature, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t getSensorTemperature(double *pTemperature) override;
    bool isTempModuleSupported() override;
    void reInit() override;
    void setSensorType(zes_temp_sensors_t sensorType);
    LinuxTemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, uint32_t sensorIndex);
    LinuxTemperatureImp() = default;
    ~LinuxTemperatureImp() override = default;

  protected:
    zes_temp_sensors_t type = ZES_TEMP_SENSORS_GLOBAL;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

  private:
    ze_result_t getMaxTemperature(double &temperature);
    const std::string &getMaxTemperatureFile() const;
    bool isHwmonTemperatureFileAvailable(NodeName temperatureNode);
    void init();
    uint32_t subdeviceId = 0;
    uint32_t sensorIndex = 0;
    ze_bool_t isSubdevice = 0;
    FsAccessInterface *pFsAccess = nullptr;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    std::string packageMaxTemperatureFile = {};
    std::string memoryMaxTemperatureFile = {};
};

} // namespace Sysman
} // namespace L0
