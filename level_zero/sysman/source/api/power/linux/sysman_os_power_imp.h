/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/power/sysman_os_power.h"

#include <memory>
#include <mutex>
#include <string>

namespace L0 {
namespace Sysman {

class SysFsAccessInterface;
class SysmanKmdInterface;
class SysmanProductHelper;
class LinuxSysmanImp;
class LinuxPowerImp : public OsPower, NEO::NonCopyableOrMovableClass {
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
    bool isIntelGraphicsHwmonDir(const std::string &name);
    ze_result_t getPmtEnergyCounter(zes_power_energy_counter_t *pEnergy);
    LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId, zes_power_domain_t powerDomain);
    LinuxPowerImp() = default;
    ~LinuxPowerImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    SysmanKmdInterface *pSysmanKmdInterface = nullptr;
    SysmanProductHelper *pSysmanProductHelper = nullptr;
    bool isTelemetrySupportAvailable = false;

  private:
    std::string intelGraphicsHwmonDir = {};
    std::string criticalPowerLimit = {};
    std::string sustainedPowerLimit = {};
    std::string sustainedPowerLimitInterval = {};
    bool canControl = false;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    uint32_t powerLimitCount = 0;
    zes_power_domain_t powerDomain = ZES_POWER_DOMAIN_CARD;

    ze_result_t getErrorCode(ze_result_t result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }

    ze_result_t getDefaultLimit(int32_t &defaultLimit);
};
} // namespace Sysman
} // namespace L0
