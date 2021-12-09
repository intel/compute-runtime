/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/core/source/device/device.h"
#include <level_zero/zes_api.h>

#include <vector>

struct _zet_sysman_pwr_handle_t {
    virtual ~_zet_sysman_pwr_handle_t() = default;
};

struct _zes_pwr_handle_t {
    virtual ~_zes_pwr_handle_t() = default;
};

namespace L0 {

struct OsSysman;
class Power : _zet_sysman_pwr_handle_t, _zes_pwr_handle_t {
  public:
    virtual ze_result_t powerGetProperties(zes_power_properties_t *pProperties) = 0;
    virtual ze_result_t powerGetEnergyCounter(zes_power_energy_counter_t *pEnergy) = 0;
    virtual ze_result_t powerGetLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerSetLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerGetEnergyThreshold(zes_energy_threshold_t *pThreshold) = 0;
    virtual ze_result_t powerSetEnergyThreshold(double threshold) = 0;

    static Power *fromHandle(zes_pwr_handle_t handle) {
        return static_cast<Power *>(handle);
    }
    inline zes_pwr_handle_t toHandle() { return this; }
    bool initSuccess = false;
    zes_power_properties_t powerProperties = {};
};
struct PowerHandleContext {
    PowerHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PowerHandleContext();

    ze_result_t init(std::vector<ze_device_handle_t> &deviceHandles, ze_device_handle_t coreDevice);
    ze_result_t powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower);

    OsSysman *pOsSysman = nullptr;
    std::vector<Power *> handleList = {};

  private:
    void createHandle(ze_device_handle_t deviceHandle);
};

} // namespace L0