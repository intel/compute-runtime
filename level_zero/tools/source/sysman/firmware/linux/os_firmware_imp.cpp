/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

namespace L0 {

bool LinuxFirmwareImp::isFirmwareSupported(void) {
    return false;
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman) {
}

OsFirmware *OsFirmware::create(OsSysman *pOsSysman) {
    LinuxFirmwareImp *pLinuxFirmwareImp = new LinuxFirmwareImp(pOsSysman);
    return static_cast<OsFirmware *>(pLinuxFirmwareImp);
}

} // namespace L0
