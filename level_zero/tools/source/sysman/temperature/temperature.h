/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {

struct OsSysman;
class Temperature : _zes_temp_handle_t {
  public:
    virtual ~Temperature() = default;
    virtual ze_result_t temperatureGetProperties(zes_temp_properties_t *pProperties) = 0;
    virtual ze_result_t temperatureGetConfig(zes_temp_config_t *pConfig) = 0;
    virtual ze_result_t temperatureSetConfig(const zes_temp_config_t *pConfig) = 0;
    virtual ze_result_t temperatureGetState(double *pTemperature) = 0;

    static Temperature *fromHandle(zes_temp_handle_t handle) {
        return static_cast<Temperature *>(handle);
    }
    inline zes_temp_handle_t toHandle() { return this; }
    bool initSuccess = false;
    zes_temp_properties_t tempProperties = {};
};

struct TemperatureHandleContext {
    TemperatureHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~TemperatureHandleContext();

    void init(std::vector<ze_device_handle_t> &deviceHandles);

    ze_result_t temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature);
    void releaseTemperatureHandles();

    OsSysman *pOsSysman = nullptr;
    std::vector<std::unique_ptr<Temperature>> handleList = {};

    bool isTempInitDone() {
        return tempInitDone;
    }

  private:
    void createHandle(const ze_device_handle_t &deviceHandle, zes_temp_sensors_t type);
    std::once_flag initTemperatureOnce;
    bool tempInitDone = false;
};

} // namespace L0
