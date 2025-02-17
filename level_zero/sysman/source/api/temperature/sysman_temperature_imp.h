/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/temperature/sysman_os_temperature.h"
#include "level_zero/sysman/source/api/temperature/sysman_temperature.h"

namespace L0 {
namespace Sysman {
class TemperatureImp : public Temperature, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t temperatureGetProperties(zes_temp_properties_t *pProperties) override;
    ze_result_t temperatureGetConfig(zes_temp_config_t *pConfig) override;
    ze_result_t temperatureSetConfig(const zes_temp_config_t *pConfig) override;
    ze_result_t temperatureGetState(double *pTemperature) override;

    TemperatureImp() = default;
    TemperatureImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_temp_sensors_t type);
    ~TemperatureImp() override;

    std::unique_ptr<OsTemperature> pOsTemperature = nullptr;
    void init();
};
} // namespace Sysman
} // namespace L0
