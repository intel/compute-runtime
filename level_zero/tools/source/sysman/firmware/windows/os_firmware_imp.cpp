/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/windows/os_firmware_imp.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {
const std::vector<std ::string> deviceSupportedFwTypes = {"GSC", "OptionROM"};

ze_result_t WddmFirmwareImp::getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties) {
    std::string fwVersion;
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (ZE_RESULT_SUCCESS == result) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    return result;
}

void WddmFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
    pProperties->canControl = true; // Assuming that user has permission to flash the firmware
}
ze_result_t WddmFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

WddmFirmwareImp::WddmFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pFwInterface = pWddmSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<WddmFirmwareImp> pWddmFirmwareImp = std::make_unique<WddmFirmwareImp>(pOsSysman, fwType);
    return pWddmFirmwareImp;
}

ze_result_t OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    WddmSysmanImp *pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pWddmSysmanImp->getFwUtilInterface();
    if (pFwInterface != nullptr) {
        supportedFwTypes = deviceSupportedFwTypes;
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
