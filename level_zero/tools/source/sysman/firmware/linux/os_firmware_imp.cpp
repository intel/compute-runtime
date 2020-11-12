/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

namespace L0 {

bool LinuxFirmwareImp::isFirmwareSupported(void) {
    if (pFwInterface != nullptr) {
        isFWInitalized = ((ZE_RESULT_SUCCESS == pFwInterface->fwDeviceInit()) ? true : false);
    }
    return isFWInitalized;
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (isFWInitalized) {
        getFirmwareVersion(pProperties->name);
        getFirmwareVersion(pProperties->version);
    } else {
        std::strncpy(pProperties->name, unknown.c_str(), (ZES_STRING_PROPERTY_SIZE - 1));
        std::strncpy(pProperties->version, unknown.c_str(), (ZES_STRING_PROPERTY_SIZE - 1));
    }
}
void LinuxFirmwareImp::getFirmwareVersion(char *firmwareVersion) {
    std::string fwVersion;
    pFwInterface->fwGetVersion(fwVersion);
    std::strncpy(firmwareVersion, fwVersion.c_str(), (ZES_STRING_PROPERTY_SIZE - 1));
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

OsFirmware *OsFirmware::create(OsSysman *pOsSysman) {
    LinuxFirmwareImp *pLinuxFirmwareImp = new LinuxFirmwareImp(pOsSysman);
    return static_cast<OsFirmware *>(pLinuxFirmwareImp);
}

} // namespace L0
