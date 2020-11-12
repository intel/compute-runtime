/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/windows/os_firmware_imp.h"

namespace L0 {

bool WddmFirmwareImp::isFirmwareSupported(void) {
    return false;
}

void WddmFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties){};

OsFirmware *OsFirmware::create(OsSysman *pOsSysman) {
    WddmFirmwareImp *pWddmFirmwareImp = new WddmFirmwareImp();
    return static_cast<OsFirmware *>(pWddmFirmwareImp);
}

} // namespace L0
