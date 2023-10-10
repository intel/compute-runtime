/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

#include <memory>

namespace L0 {
namespace Sysman {

struct OsSysman;
class OsFan {
  public:
    virtual ze_result_t getProperties(zes_fan_properties_t *pProperties) = 0;
    virtual ze_result_t getConfig(zes_fan_config_t *pConfig) = 0;
    virtual ze_result_t setDefaultMode() = 0;
    virtual ze_result_t setFixedSpeedMode(const zes_fan_speed_t *pSpeed) = 0;
    virtual ze_result_t setSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) = 0;
    virtual ze_result_t getState(zes_fan_speed_units_t units, int32_t *pSpeed) = 0;
    virtual bool isFanModuleSupported() = 0;
    static std::unique_ptr<OsFan> create(OsSysman *pOsSysman);
    virtual ~OsFan() = default;
};

} // namespace Sysman
} // namespace L0
