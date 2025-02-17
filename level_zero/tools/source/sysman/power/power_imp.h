/*
 * Copyright (C) 2020-2025 Intel Corporation
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
class PowerImp : public Power, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t powerGetProperties(zes_power_properties_t *pProperties) override;
    ze_result_t powerGetEnergyCounter(zes_power_energy_counter_t *pEnergy) override;
    ze_result_t powerGetLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) override;
    ze_result_t powerSetLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) override;
    ze_result_t powerGetEnergyThreshold(zes_energy_threshold_t *pThreshold) override;
    ze_result_t powerSetEnergyThreshold(double threshold) override;
    ze_result_t powerGetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) override;
    ze_result_t powerSetLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) override;

    PowerImp() = default;
    PowerImp(OsSysman *pOsSysman, ze_device_handle_t device);
    ~PowerImp() override;

    OsPower *pOsPower = nullptr;
    void init();

  private:
    ze_device_handle_t deviceHandle = {};
    zes_power_properties_t powerProperties = {};
};
} // namespace L0
