/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/standby/linux/os_standby_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace L0 {

ze_result_t LinuxStandbyImp::osStandbyGetProperties(zes_standby_properties_t &properties) {
    properties.pNext = nullptr;
    properties.type = ZES_STANDBY_TYPE_GLOBAL;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}

bool LinuxStandbyImp::isStandbySupported(void) {
    auto rel = pSysfsAccess->canRead(standbyModeFile);
    if (ZE_RESULT_SUCCESS == rel) {
        return true;
    } else {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <can't read file %s> <error: 0x%x>\n", __func__, standbyModeFile.c_str(), rel);
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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <failed to read file %s> <result: 0x%x>\n", __func__, standbyModeFile.c_str(), result);
        return result;
    }
    if (standbyModeDefault == currentMode) {
        mode = ZES_STANDBY_PROMO_MODE_DEFAULT;
    } else if (standbyModeNever == currentMode) {
        mode = ZES_STANDBY_PROMO_MODE_NEVER;
    } else {
        result = ZE_RESULT_ERROR_UNKNOWN;
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <unknown or internal error occurred> <currentMode: %d & result: 0x%x>\n", __func__, currentMode, result);
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
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "error@<%s> <Unsupported feature> <result: 0x%x>\n", __func__, result);
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

std::unique_ptr<OsStandby> OsStandby::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxStandbyImp> pLinuxStandbyImp = std::make_unique<LinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    return pLinuxStandbyImp;
}

} // namespace L0
