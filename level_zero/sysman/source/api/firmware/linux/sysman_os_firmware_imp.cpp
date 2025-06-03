/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

void OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    supportedFwTypes.clear();
    if (pFwInterface != nullptr) {
        auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
        pSysmanProductHelper->getDeviceSupportedFwTypes(pFwInterface, supportedFwTypes);
        if (pSysmanProductHelper->isLateBindingSupported()) {
            pFwInterface->getLateBindingSupportedFwTypes(supportedFwTypes);
        }
    }
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.data(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    pProperties->canControl = true; // Assuming that user has permission to flash the firmware
}

ze_result_t LinuxFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

ze_result_t LinuxFirmwareImp::osGetSecurityVersion(char *pVersion) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osSetSecurityVersion() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osGetConsoleLogs(size_t *pSize, char *pFirmwareLog) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osGetFirmwareFlashProgress(uint32_t *pCompletionPercent) {
    return pFwInterface->getFlashFirmwareProgress(pCompletionPercent);
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<LinuxFirmwareImp> pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, fwType);
    return pLinuxFirmwareImp;
}

} // namespace Sysman
} // namespace L0
