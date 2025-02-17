/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/power/sysman_os_power.h"

namespace L0 {
namespace Sysman {
class KmdSysManager;
class WddmSysmanImp;
class WddmPowerImp : public OsPower, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_power_properties_t *pProperties) override;
    ze_result_t getEnergyCounter(zes_power_energy_counter_t *pEnergy) override;
    ze_result_t getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) override;
    ze_result_t setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) override;
    ze_result_t getEnergyThreshold(zes_energy_threshold_t *pThreshold) override;
    ze_result_t setEnergyThreshold(double threshold) override;
    ze_result_t getLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) override;
    ze_result_t setLimitsExt(uint32_t *pCount, zes_power_limit_ext_desc_t *pSustained) override;
    ze_result_t getPropertiesExt(zes_power_ext_properties_t *pExtPoperties) override;

    bool isPowerModuleSupported() override;
    void isPowerHandleEnergyCounterOnly();
    void initPowerLimits();
    WddmPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain);
    WddmPowerImp() = default;
    ~WddmPowerImp() override = default;

  protected:
    KmdSysManager *pKmdSysManager = nullptr;
    WddmSysmanImp *pWddmSysmanImp = nullptr;
    uint32_t powerLimitCount = 0;
    zes_power_domain_t powerDomain = ZES_POWER_DOMAIN_CARD;
    bool supportsEnergyCounterOnly = false;
};

} // namespace Sysman
} // namespace L0
