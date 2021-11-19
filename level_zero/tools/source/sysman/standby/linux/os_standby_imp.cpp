/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"

namespace L0 {

ze_result_t LinuxStandbyImp::osStandbyGetProperties(zes_standby_properties_t &properties) {
    properties.pNext = nullptr;
    properties.type = ZES_STANDBY_TYPE_GLOBAL;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}

bool LinuxStandbyImp::isStandbySupported(void) {
    if (ZE_RESULT_SUCCESS == pSysfsAccess->canRead(standbyModeFile)) {
        return true;
    } else {
        return false;
    }
}

ze_result_t LinuxStandbyImp::getMode(zes_standby_promo_mode_t &mode) {
    int currentMode = -1;
    ze_result_t result = pSysfsAccess->read(standbyModeFile, currentMode);
    if (ZE_RESULT_SUCCESS != result) {
        if (result == ZE_RESULT_ERROR_NOT_AVAILABLE) {
            result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
        return result;
    }
    if (standbyModeDefault == currentMode) {
        mode = ZES_STANDBY_PROMO_MODE_DEFAULT;
    } else if (standbyModeNever == currentMode) {
        mode = ZES_STANDBY_PROMO_MODE_NEVER;
    } else {
        result = ZE_RESULT_ERROR_UNKNOWN;
    }
    return result;
}

ze_result_t LinuxStandbyImp::setMode(zes_standby_promo_mode_t mode) {
    ze_result_t result = ZE_RESULT_ERROR_UNKNOWN;
    if (ZES_STANDBY_PROMO_MODE_DEFAULT == mode) {
        result = pSysfsAccess->write(standbyModeFile, standbyModeDefault);
    } else {
        result = pSysfsAccess->write(standbyModeFile, standbyModeNever);
    }

    if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    return result;
}

void LinuxStandbyImp::init() {
    const std::string baseDir = "gt/gt" + std::to_string(subdeviceId) + "/";
    if (pSysfsAccess->directoryExists(baseDir)) {
        standbyModeFile = baseDir + "rc6_enable";
    } else {
        standbyModeFile = "power/rc6_enable";
    }
}

LinuxStandbyImp::LinuxStandbyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    init();
}

OsStandby *OsStandby::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    LinuxStandbyImp *pLinuxStandbyImp = new LinuxStandbyImp(pOsSysman, onSubdevice, subdeviceId);
    return static_cast<OsStandby *>(pLinuxStandbyImp);
}

} // namespace L0
