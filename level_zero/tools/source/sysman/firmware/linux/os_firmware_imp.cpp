/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

#include "shared/source/helpers/string.h"

namespace L0 {

static const std::string mtdDescriptor("/proc/mtd");

std::vector<std ::string> deviceSupportedFwTypes = {"GSC", "OptionROM"};

ze_result_t OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    FsAccess *pFsAccess = &pLinuxSysmanImp->getFsAccess();
    std::vector<std::string> mtdDescriptorStrings = {};
    ze_result_t result = pFsAccess->read(mtdDescriptor, mtdDescriptorStrings);
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    for (const auto &readByteLine : mtdDescriptorStrings) {
        for (const auto &fwType : deviceSupportedFwTypes) {
            if (std::string::npos != readByteLine.find(fwType)) {
                supportedFwTypes.push_back(fwType);
            }
        }
    }
    return ZE_RESULT_SUCCESS;
}
bool LinuxFirmwareImp::isFirmwareSupported(void) {
    if (pFwInterface != nullptr) {
        isFWInitalized = ((ZE_RESULT_SUCCESS == pFwInterface->fwDeviceInit()) ? true : false);
        return this->isFWInitalized;
    }
    return false;
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (osFwType == deviceSupportedFwTypes[0]) { //GSC
        getFirmwareVersion(pProperties->version);
    }
    if (osFwType == deviceSupportedFwTypes[1]) { //oprom
        getOpromVersion(pProperties->version);
    }
}
ze_result_t LinuxFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    if (osFwType == deviceSupportedFwTypes[0]) { //GSC
        return pFwInterface->fwFlashGSC(pImage, size);
    }
    if (osFwType == deviceSupportedFwTypes[1]) { //oprom
        return pFwInterface->fwFlashOprom(pImage, size);
    }
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

void LinuxFirmwareImp::getFirmwareVersion(char *firmwareVersion) {
    std::string fwVersion;
    if (ZE_RESULT_SUCCESS == pFwInterface->fwGetVersion(fwVersion)) {
        strncpy_s(firmwareVersion, ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
    } else {
        strncpy_s(firmwareVersion, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
}

void LinuxFirmwareImp::getOpromVersion(char *firmwareVersion) {
    std::string fwVersion;
    if (ZE_RESULT_SUCCESS == pFwInterface->opromGetVersion(fwVersion)) {
        strncpy_s(firmwareVersion, ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
    } else {
        strncpy_s(firmwareVersion, ZES_STRING_PROPERTY_SIZE, unknown.c_str(), ZES_STRING_PROPERTY_SIZE);
    }
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<LinuxFirmwareImp> pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, fwType);
    return pLinuxFirmwareImp;
}

} // namespace L0
