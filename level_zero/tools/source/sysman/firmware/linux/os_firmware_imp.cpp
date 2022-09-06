/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"

namespace L0 {

static const std::string mtdDescriptor("/proc/mtd");

ze_result_t OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    std::vector<std ::string> deviceSupportedFwTypes;
    if (pFwInterface != nullptr) {
        pFwInterface->getDeviceSupportedFwTypes(deviceSupportedFwTypes);
    }

    FsAccess *pFsAccess = &pLinuxSysmanImp->getFsAccess();
    std::vector<std::string> mtdDescriptorStrings = {};
    ze_result_t result = pFsAccess->read(mtdDescriptor, mtdDescriptorStrings);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    for (const auto &readByteLine : mtdDescriptorStrings) {
        for (const auto &fwType : deviceSupportedFwTypes) {
            if (std::string::npos != readByteLine.find(fwType)) {
                if (std::find(supportedFwTypes.begin(), supportedFwTypes.end(), fwType) == supportedFwTypes.end()) {
                    supportedFwTypes.push_back(fwType);
                }
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    pProperties->canControl = true; //Assuming that user has permission to flash the firmware
}

ze_result_t LinuxFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<LinuxFirmwareImp> pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, fwType);
    return pLinuxFirmwareImp;
}

} // namespace L0
