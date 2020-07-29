/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/temperature/os_temperature.h"
#include "level_zero/tools/source/sysman/temperature/temperature.h"
namespace L0 {
class TemperatureImp : public Temperature, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t temperatureGetProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t temperatureGetConfig(zes_temp_config_t *pConfig) override;
    ze_result_t temperatureSetConfig(const zes_temp_config_t *pConfig) override;
    ze_result_t temperatureGetState(double *pTemperature) override;

    TemperatureImp() = default;
    TemperatureImp(OsSysman *pOsSysman, zes_temp_sensors_t type);
    ~TemperatureImp() override;

    OsTemperature *pOsTemperature = nullptr;
    void init();
};
} // namespace L0
