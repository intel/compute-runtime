/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/power/os_power.h"

#include <memory>
#include <string>

namespace L0 {

class SysfsAccess;
class PlatformMonitoringTech;
class LinuxPowerImp : public OsPower, NEO::NonCopyableAndNonMovableClass {
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
    bool isHwmonDir(std::string name);
    ze_result_t getPmtEnergyCounter(zes_power_energy_counter_t *pEnergy);
    LinuxPowerImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxPowerImp() = default;
    ~LinuxPowerImp() override = default;

  protected:
    PlatformMonitoringTech *pPmt = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;

  private:
    std::string i915HwmonDir;
    std::string energyHwmonDir;
    static const std::string hwmonDir;
    static const std::string i915;
    static const std::string sustainedPowerLimitEnabled;
    static const std::string sustainedPowerLimit;
    static const std::string sustainedPowerLimitInterval;
    static const std::string burstPowerLimitEnabled;
    static const std::string burstPowerLimit;
    static const std::string energyCounterNode;
    static const std::string defaultPowerLimit;
    static const std::string minPowerLimit;
    static const std::string maxPowerLimit;
    bool canControl = false;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;

    ze_result_t getErrorCode(ze_result_t result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
};
} // namespace L0
