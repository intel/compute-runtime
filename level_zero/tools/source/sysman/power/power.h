/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zet_api.h>

#include <vector>

struct _zet_sysman_pwr_handle_t {
    virtual ~_zet_sysman_pwr_handle_t() = default;
};

namespace L0 {

struct OsSysman;
class Power : _zet_sysman_pwr_handle_t {
  public:
    virtual ze_result_t powerGetProperties(zet_power_properties_t *pProperties) = 0;
    virtual ze_result_t powerGetEnergyCounter(zet_power_energy_counter_t *pEnergy) = 0;
    virtual ze_result_t powerGetLimits(zet_power_sustained_limit_t *pSustained, zet_power_burst_limit_t *pBurst, zet_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerSetLimits(const zet_power_sustained_limit_t *pSustained, const zet_power_burst_limit_t *pBurst, const zet_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t powerGetEnergyThreshold(zet_energy_threshold_t *pThreshold) = 0;
    virtual ze_result_t powerSetEnergyThreshold(double threshold) = 0;

    static Power *fromHandle(zet_sysman_pwr_handle_t handle) {
        return static_cast<Power *>(handle);
    }
    inline zet_sysman_pwr_handle_t toHandle() { return this; }
    bool initSuccess = false;
};
struct PowerHandleContext {
    PowerHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~PowerHandleContext();

    void init();

    ze_result_t powerGet(uint32_t *pCount, zet_sysman_pwr_handle_t *phPower);

    OsSysman *pOsSysman = nullptr;
    std::vector<Power *> handleList;
};

} // namespace L0