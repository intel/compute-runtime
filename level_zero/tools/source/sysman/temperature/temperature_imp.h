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
#include <level_zero/zet_api.h>
namespace L0 {
class TemperatureImp : public NEO::NonCopyableClass, public Temperature {
  public:
    ze_result_t temperatureGetProperties(zet_temp_properties_t *pProperties) override;
    ze_result_t temperatureGetConfig(zet_temp_config_t *pConfig) override;
    ze_result_t temperatureSetConfig(const zet_temp_config_t *pConfig) override;
    ze_result_t temperatureGetState(double *pTemperature) override;

    TemperatureImp() = default;
    TemperatureImp(OsSysman *pOsSysman);
    ~TemperatureImp() override;

    OsTemperature *pOsTemperature = nullptr;
    void init() {}
};
} // namespace L0
