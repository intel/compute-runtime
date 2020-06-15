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

void LinuxRasImp::setRasErrorType(zet_ras_error_type_t type) {
    osRasErrorType = type;
}
bool LinuxRasImp::isRasSupported(void) {
    if (false == pFsAccess->fileExists(rasCounterDir)) {
        return false;
    }
    if (osRasErrorType == ZET_RAS_ERROR_TYPE_CORRECTABLE) {
        return false;
    } else {
        // i915 support for UNCORRECTABLE errors is assumed true
        // since support for reset event is already available.
        return true;
    }
}
ze_result_t LinuxRasImp::getCounterValues(zet_ras_details_t *pDetails) {
    uint64_t counterValue = 0;
    ze_result_t result = pFsAccess->read(resetCounterFile, counterValue);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pDetails->numResets = counterValue;
    return result;
}

LinuxRasImp::LinuxRasImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFsAccess = &pLinuxSysmanImp->getFsAccess();
}

OsRas *OsRas::create(OsSysman *pOsSysman) {
    LinuxRasImp *pLinuxRasImp = new LinuxRasImp(pOsSysman);
    return static_cast<OsRas *>(pLinuxRasImp);
}

} // namespace L0
