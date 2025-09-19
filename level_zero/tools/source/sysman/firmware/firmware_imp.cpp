/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "firmware_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "os_firmware.h"

namespace L0 {

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
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

FirmwareImp::FirmwareImp(OsSysman *pOsSysman, const std::string &initializedFwType) {
    pOsFirmware = OsFirmware::create(pOsSysman, initializedFwType);
    fwType = initializedFwType;
    UNRECOVERABLE_IF(nullptr == pOsFirmware);
}

FirmwareImp::~FirmwareImp() {
}

} // namespace L0
