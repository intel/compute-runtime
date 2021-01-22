/*
 * Copyright (C) 2020-2021 Intel Corporation
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
ze_result_t WddmFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
};

OsFirmware *OsFirmware::create(OsSysman *pOsSysman) {
    WddmFirmwareImp *pWddmFirmwareImp = new WddmFirmwareImp();
    return static_cast<OsFirmware *>(pWddmFirmwareImp);
}

} // namespace L0
