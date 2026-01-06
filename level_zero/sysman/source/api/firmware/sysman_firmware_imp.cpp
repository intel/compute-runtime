/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/sysman_firmware_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/sysman/source/api/firmware/sysman_os_firmware.h"

namespace L0 {
namespace Sysman {

ze_result_t FirmwareImp::firmwareGetProperties(zes_firmware_properties_t *pProperties) {
    pOsFirmware->osGetFwProperties(pProperties);
    std::string fwName = fwType;
    if (fwName == "GSC") {
        fwName = "GFX";
    }
    strncpy_s(pProperties->name, ZES_STRING_PROPERTY_SIZE, fwName.c_str(), fwName.size());
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareImp::firmwareFlash(void *pImage, uint32_t size) {
    return pOsFirmware->osFirmwareFlash(pImage, size);
}

ze_result_t FirmwareImp::firmwareGetFlashProgress(uint32_t *pCompletionPercent) {
    return pOsFirmware->osGetFirmwareFlashProgress(pCompletionPercent);
}

ze_result_t FirmwareImp::firmwareGetSecurityVersion(char *pVersion) {
    return pOsFirmware->osGetSecurityVersion(pVersion);
}

ze_result_t FirmwareImp::firmwareSetSecurityVersion() {
    return pOsFirmware->osSetSecurityVersion();
}

ze_result_t FirmwareImp::firmwareGetConsoleLogs(size_t *pSize, char *pFirmwareLog) {
    return pOsFirmware->osGetConsoleLogs(pSize, pFirmwareLog);
}

FirmwareImp::FirmwareImp(OsSysman *pOsSysman, const std::string &initializedFwType) {
    pOsFirmware = OsFirmware::create(pOsSysman, initializedFwType);
    fwType = initializedFwType;
    UNRECOVERABLE_IF(nullptr == pOsFirmware);
}

FirmwareImp::~FirmwareImp() {
}

} // namespace Sysman
} // namespace L0
