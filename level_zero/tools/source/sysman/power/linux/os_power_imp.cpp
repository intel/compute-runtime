/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

#include "level_zero/tools/source/sysman/linux/pmt.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

void powerGetTimestamp(uint64_t &timestamp) {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    timestamp = std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t LinuxPowerImp::getProperties(zes_power_properties_t *pProperties) {
    pProperties->onSubdevice = false;
    pProperties->subdeviceId = 0;
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxPowerImp::getEnergyCounter(zes_power_energy_counter_t *pEnergy) {
    const std::string key("PACKAGE_ENERGY");
    uint64_t energy = 0;
    ze_result_t result = pPmt->readValue(key, energy);
    // PMT will return in joules and need to convert into microjoules
    pEnergy->energy = energy * convertJouleToMicroJoule;

    powerGetTimestamp(pEnergy->timestamp);

    return result;
}

ze_result_t LinuxPowerImp::getLimits(zes_power_sustained_limit_t *pSustained, zes_power_burst_limit_t *pBurst, zes_power_peak_limit_t *pPeak) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setLimits(const zes_power_sustained_limit_t *pSustained, const zes_power_burst_limit_t *pBurst, const zes_power_peak_limit_t *pPeak) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::getEnergyThreshold(zes_energy_threshold_t *pThreshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    return pPmt->isPmtSupported();
}
LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pPmt = &pLinuxSysmanImp->getPlatformMonitoringTechAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0
