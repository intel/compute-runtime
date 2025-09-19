/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/standby/linux/sysman_os_standby_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxStandbyImp::osStandbyGetProperties(zes_standby_properties_t &properties) {
    properties.pNext = nullptr;
    properties.type = ZES_STANDBY_TYPE_GLOBAL;
    properties.onSubdevice = isSubdevice;
    properties.subdeviceId = subdeviceId;
    return ZE_RESULT_SUCCESS;
}

bool LinuxStandbyImp::isStandbySupported(void) {
    if ((!standbyModeFile.empty()) && (ZE_RESULT_SUCCESS == pSysfsAccess->canRead(standbyModeFile))) {
        return true;
    }
    return false;
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
    const std::string baseDir = pSysmanKmdInterface->getBasePath(subdeviceId);
    bool baseDirectoryExists = false;

    if (pSysfsAccess->directoryExists(baseDir)) {
        baseDirectoryExists = true;
    }

    if (pSysmanProductHelper->isStandbySupported(pSysmanKmdInterface)) {
        standbyModeFile = pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, subdeviceId, baseDirectoryExists);
    }
}

LinuxStandbyImp::LinuxStandbyImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : isSubdevice(onSubdevice), subdeviceId(subdeviceId) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    init();
}

std::unique_ptr<OsStandby> OsStandby::create(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) {
    std::unique_ptr<LinuxStandbyImp> pLinuxStandbyImp = std::make_unique<LinuxStandbyImp>(pOsSysman, onSubdevice, subdeviceId);
    return pLinuxStandbyImp;
}

} // namespace Sysman
} // namespace L0
