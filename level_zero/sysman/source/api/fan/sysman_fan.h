/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <memory>
#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {
struct OsSysman;
class Fan : _zes_fan_handle_t {
  public:
    virtual ~Fan() = default;
    virtual ze_result_t fanGetProperties(zes_fan_properties_t *pProperties) = 0;
    virtual ze_result_t fanGetConfig(zes_fan_config_t *pConfig) = 0;
    virtual ze_result_t fanSetDefaultMode() = 0;
    virtual ze_result_t fanSetFixedSpeedMode(const zes_fan_speed_t *pSpeed) = 0;
    virtual ze_result_t fanSetSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) = 0;
    virtual ze_result_t fanGetState(zes_fan_speed_units_t units, int32_t *pSpeed) = 0;

    static Fan *fromHandle(zes_fan_handle_t handle) {
        return static_cast<Fan *>(handle);
    }
    inline zes_fan_handle_t toHandle() { return this; }
    bool initSuccess = false;
};
struct FanHandleContext {
    FanHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FanHandleContext();

    void init();

    ze_result_t fanGet(uint32_t *pCount, zes_fan_handle_t *phFan);

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Fan>> handleList = {};

  private:
    std::once_flag initFanOnce;
};

} // namespace Sysman
} // namespace L0
