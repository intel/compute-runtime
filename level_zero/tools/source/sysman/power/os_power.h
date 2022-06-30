/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <level_zero/zes_api.h>

namespace L0 {

struct OsSysman;
class OsPower {
  public:
    virtual ze_result_t getProperties(zes_power_properties_t *pProperties) = 0;
    virtual ze_result_t getEnergyCounter(zes_power_energy_counter_t *pEnergy) = 0;
    virtual ze_result_t getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) = 0;
    virtual ze_result_t getEnergyThreshold(zes_energy_threshold_t *pThreshold) = 0;
    virtual ze_result_t setEnergyThreshold(double threshold) = 0;

    virtual bool isPowerModuleSupported() = 0;
    static OsPower *create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~OsPower() = default;
};

} // namespace L0
