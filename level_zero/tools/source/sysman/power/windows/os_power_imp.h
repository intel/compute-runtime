/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/power/os_power.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {
class KmdSysManager;
class WddmPowerImp : public OsPower, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_power_properties_t *pProperties) override;
    ze_result_t getEnergyCounter(zes_power_energy_counter_t *pEnergy) override;
    ze_result_t getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) override;
    ze_result_t setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) override;
    ze_result_t getEnergyThreshold(zes_energy_threshold_t *pThreshold) override;
    ze_result_t setEnergyThreshold(double threshold) override;

    bool isPowerModuleSupported() override;
    WddmPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    WddmPowerImp() = default;
    ~WddmPowerImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
};

} // namespace L0
