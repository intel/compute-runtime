/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/temperature/os_temperature.h"
#include "level_zero/tools/source/sysman/temperature/temperature.h"
namespace L0 {
class TemperatureImp : public Temperature, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t temperatureGetProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t temperatureGetConfig(zes_temp_config_t *pConfig) override;
    ze_result_t temperatureSetConfig(const zes_temp_config_t *pConfig) override;
    ze_result_t temperatureGetState(double *pTemperature) override;

    TemperatureImp() = default;
    TemperatureImp(const ze_device_handle_t &deviceHandle, OsSysman *pOsSysman, zes_temp_sensors_t type);
    ~TemperatureImp() override;

    std::unique_ptr<OsTemperature> pOsTemperature = nullptr;
    void init();
};
} // namespace L0
