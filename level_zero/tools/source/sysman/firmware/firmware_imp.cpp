/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "firmware_imp.h"

#include "shared/source/helpers/debug_helpers.h"

#include "os_firmware.h"

#include <cmath>

namespace L0 {

ze_result_t FirmwareImp::firmwareGetProperties(zes_firmware_properties_t *pProperties) {
    pOsFirmware->osGetFwProperties(pProperties);
    strncpy_s(pProperties->name, ZES_STRING_PROPERTY_SIZE, fwType.c_str(), fwType.size());
    return ZE_RESULT_SUCCESS;
}

ze_result_t FirmwareImp::firmwareFlash(void *pImage, uint32_t size) {
    return pOsFirmware->osFirmwareFlash(pImage, size);
}

void FirmwareImp::init() {
    this->isFirmwareEnabled = pOsFirmware->isFirmwareSupported();
}

FirmwareImp::FirmwareImp(OsSysman *pOsSysman, const std::string &initalizedFwType) {
    pOsFirmware = OsFirmware::create(pOsSysman, initalizedFwType);
    fwType = initalizedFwType;
    UNRECOVERABLE_IF(nullptr == pOsFirmware);
    init();
}

FirmwareImp::~FirmwareImp() {
}

} // namespace L0
