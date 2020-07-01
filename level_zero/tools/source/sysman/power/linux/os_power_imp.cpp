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
constexpr uint64_t convertJouleToMicroJoule = 1000000u;
ze_result_t LinuxPowerImp::getEnergyThreshold(zet_energy_threshold_t *pThreshold) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxPowerImp::setEnergyThreshold(double threshold) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}
ze_result_t LinuxPowerImp::getEnergyCounter(uint64_t &energy) {
    const std::string key("PACKAGE_ENERGY");
    ze_result_t result = pPmt->readValue(key, energy);
    // PMT will return in joules and need to convert into microjoules
    energy = energy * convertJouleToMicroJoule;
    return result;
}

bool LinuxPowerImp::isPowerModuleSupported() {
    return pPmt->isPmtSupported();
}
LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pPmt = &pLinuxSysmanImp->getPlatformMonitoringTechAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0
