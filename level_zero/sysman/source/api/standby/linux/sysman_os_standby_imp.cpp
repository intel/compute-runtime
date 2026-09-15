/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/standby/linux/sysman_os_standby_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/preprocessor.h"

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
    ze_result_t result = pSysmanProductHelper->getStandbyMode(pSysfsAccess, standbyModeFile, mode);
    if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "error@<%s> <Unsupported feature> <result: 0x%x>\n", NEO_FUNCTION_NAME, result);
    }
    return result;
}

ze_result_t LinuxStandbyImp::setMode(zes_standby_promo_mode_t mode) {
    if (!pSysmanProductHelper->isSetStandbyModeSupported()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    ze_result_t result = pSysmanProductHelper->setStandbyMode(pSysfsAccess, standbyModeFile, mode);
    if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
        result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                     "error@<%s> <Unsupported feature> <result: 0x%x>\n", NEO_FUNCTION_NAME, result);
    }
    return result;
}

void LinuxStandbyImp::reInit() {
    standbyModeFile.clear();
    init();
}

void LinuxStandbyImp::init() {
    if (pSysmanProductHelper->isStandbySupported(pSysmanKmdInterface)) {
        standbyModeFile = pSysmanProductHelper->getStandbyModeFile(pSysmanKmdInterface, pSysfsAccess, subdeviceId);
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
