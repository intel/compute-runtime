/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

#include "shared/source/helpers/string.h"

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
        strncpy_s(pProperties->name, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
        strncpy_s(pProperties->version, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
}
void LinuxFirmwareImp::getFirmwareVersion(char *firmwareVersion) {
    std::string fwVersion;
    pFwInterface->fwGetVersion(fwVersion);
    strncpy_s(firmwareVersion, ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
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
