/*
 * Copyright (C) 2020 Intel Corporation
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
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void FirmwareImp::init() {
    this->isFirmwareEnabled = pOsFirmware->isFirmwareSupported();
}

FirmwareImp::FirmwareImp(OsSysman *pOsSysman) {
    pOsFirmware = OsFirmware::create(pOsSysman);
    UNRECOVERABLE_IF(nullptr == pOsFirmware);
    init();
}

FirmwareImp::~FirmwareImp() {
    if (pOsFirmware != nullptr) {
        delete pOsFirmware;
        pOsFirmware = nullptr;
    }
}

} // namespace L0
