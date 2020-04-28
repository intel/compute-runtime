/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/power/os_power.h"
#include "level_zero/tools/source/sysman/power/power.h"
#include <level_zero/zet_api.h>
namespace L0 {
class PowerImp : public NEO::NonCopyableClass, public Power {
  public:
    ze_result_t powerGetProperties(zet_power_properties_t *pProperties) override;
    ze_result_t powerGetEnergyCounter(zet_power_energy_counter_t *pEnergy) override;
    ze_result_t powerGetLimits(zet_power_sustained_limit_t *pSustained, zet_power_burst_limit_t *pBurst, zet_power_peak_limit_t *pPeak) override;
    ze_result_t powerSetLimits(const zet_power_sustained_limit_t *pSustained, const zet_power_burst_limit_t *pBurst, const zet_power_peak_limit_t *pPeak) override;
    ze_result_t powerGetEnergyThreshold(zet_energy_threshold_t *pThreshold) override;
    ze_result_t powerSetEnergyThreshold(double threshold) override;
    PowerImp() = default;
    PowerImp(OsSysman *pOsSysman);
    ~PowerImp() override;

    OsPower *pOsPower = nullptr;
    void init() {}
};
} // namespace L0
