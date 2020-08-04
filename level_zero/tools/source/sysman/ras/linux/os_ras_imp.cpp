/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/ras/linux/os_ras_imp.h"

#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

const std::string LinuxRasImp::rasCounterDir("/var/lib/libze_intel_gpu/");
const std::string LinuxRasImp::resetCounter("ras_reset_count");
const std::string LinuxRasImp::resetCounterFile = rasCounterDir + resetCounter;

void LinuxRasImp::setRasErrorType(zes_ras_error_type_t type) {
    osRasErrorType = type;
}
bool LinuxRasImp::isRasSupported(void) {
    if (false == pFsAccess->fileExists(rasCounterDir)) {
        return false;
    }
    if (osRasErrorType == ZES_RAS_ERROR_TYPE_CORRECTABLE) {
        return false;
    } else {
        return false;
    }
}

LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
    osRasErrorType = ZES_RAS_ERROR_TYPE_UNCORRECTABLE;
}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0
