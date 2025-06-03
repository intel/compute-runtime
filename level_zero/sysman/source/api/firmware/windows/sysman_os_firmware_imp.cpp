/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/windows/sysman_os_firmware_imp.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

namespace L0 {
namespace Sysman {

const std::vector<std ::string> deviceSupportedFwTypes = {"GSC", "OptionROM"};

ze_result_t WddmFirmwareImp::getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties) {
    std::string fwVersion;
    if (std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), fwType) != lateBindingFirmwareTypes.end()) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (ZE_RESULT_SUCCESS == result) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    return result;
}

void WddmFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.data(), ZES_STRING_PROPERTY_SIZE);
    }
    pProperties->canControl = true; // Assuming that user has permission to flash the firmware
}

ze_result_t WddmFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

ze_result_t WddmFirmwareImp::osGetSecurityVersion(char *pVersion) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFirmwareImp::osSetSecurityVersion() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFirmwareImp::osGetConsoleLogs(size_t *pSize, char *pFirmwareLog) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmFirmwareImp::osGetFirmwareFlashProgress(uint32_t *pCompletionPercent) {
    return pFwInterface->getFlashFirmwareProgress(pCompletionPercent);
}

WddmFirmwareImp::WddmFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pFwInterface = pWddmSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<WddmFirmwareImp> pWddmFirmwareImp = std::make_unique<WddmFirmwareImp>(pOsSysman, fwType);
    return pWddmFirmwareImp;
}

void OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pWddmSysmanImp->getFwUtilInterface();
    auto pSysmanProductHelper = pWddmSysmanImp->getSysmanProductHelper();
    supportedFwTypes.clear();
    if (pFwInterface != nullptr) {
        supportedFwTypes = deviceSupportedFwTypes;
        if (pSysmanProductHelper->isLateBindingSupported()) {
            pFwInterface->getLateBindingSupportedFwTypes(supportedFwTypes);
        }
    }
}

} // namespace Sysman
} // namespace L0
