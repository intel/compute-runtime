/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <memory>

namespace L0 {

struct OsSysman;
class OsTemperature {
  public:
    virtual ze_result_t getProperties(zes_temp_properties_t *pProperties) = 0;
    virtual ze_result_t getSensorTemperature(double *pTemperature) = 0;
    virtual bool isTempModuleSupported() = 0;
    static std::unique_ptr<OsTemperature> create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t sensorType);
    virtual ~OsTemperature() = default;
};

} // namespace L0