/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/power/linux/os_power_imp.h"

namespace L0 {

ze_result_t LinuxPowerImp::getEnergyCounter(uint64_t &energy) {

    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

LinuxPowerImp::LinuxPowerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsPower *OsPower::create(OsSysman *pOsSysman) {
    LinuxPowerImp *pLinuxPowerImp = new LinuxPowerImp(pOsSysman);
    return static_cast<OsPower *>(pLinuxPowerImp);
}

} // namespace L0
