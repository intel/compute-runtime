/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/temperature/linux/os_temperature_imp.h"

namespace L0 {

ze_result_t LinuxTemperatureImp::getSensorTemperature(double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

LinuxTemperatureImp::LinuxTemperatureImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsTemperature *OsTemperature::create(OsSysman *pOsSysman) {
    LinuxTemperatureImp *pLinuxTemperatureImp = new LinuxTemperatureImp(pOsSysman);
    return static_cast<OsTemperature *>(pLinuxTemperatureImp);
}

} // namespace L0