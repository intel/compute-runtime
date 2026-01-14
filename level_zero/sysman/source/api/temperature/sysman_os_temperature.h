/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <map>
#include <memory>

namespace L0 {
namespace Sysman {
struct OsSysman;
class OsTemperature {
  public:
    virtual ze_result_t getProperties(zes_temp_properties_t *pProperties) = 0;
    virtual ze_result_t getSensorTemperature(double *pTemperature) = 0;
    virtual bool isTempModuleSupported() = 0;
    static void getSupportedSensors(OsSysman *pOsSysman, std::map<zes_temp_sensors_t, uint32_t> &supportedSensorTypeMap);
    static std::unique_ptr<OsTemperature> create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType, uint32_t sensorIndex);
    virtual ~OsTemperature() = default;
};

} // namespace Sysman
} // namespace L0