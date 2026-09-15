/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <sstream>

namespace L0 {
namespace Sysman {

static const std::string fdoFwType = "Flash_Override";
static const std::string procMtdPath = "/proc/mtd";
static const std::string procMtdStringPrefix = "xe.nvm.";
static const std::string procMtdStringSuffix = ".DATA";

void OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
    supportedFwTypes.clear();

    if (pLinuxSysmanImp->isDeviceInSurvivabilityMode() && pSysmanKmdInterface->isDeviceInFdoMode()) {
        supportedFwTypes.push_back(fdoFwType);
        return;
    }

    if (pFwInterface != nullptr) {
        pSysmanProductHelper->getDeviceSupportedFwTypes(pFwInterface, supportedFwTypes);
        pSysmanKmdInterface->getLateBindingSupportedFwTypes(supportedFwTypes);
    }
    if (pSysmanProductHelper->isFlashOverrideSupported()) {
        supportedFwTypes.push_back(fdoFwType);
    }
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.data(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    pProperties->canControl = true; // Assuming that user has permission to flash the firmware
}

ze_result_t LinuxFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    // Only the flash override firmware can be flashed while the device is in fdo mode
    if (pSysmanKmdInterface->isDeviceInFdoMode() && osFwType != fdoFwType) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }

    if (osFwType == fdoFwType) {
        return osFirmwareFlashExtended(pImage, size);
    }
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

ze_result_t LinuxFirmwareImp::osFirmwareFlashExtended(void *pImage, uint32_t size) {
    // Validate the image before touching the device, since the device is erased before it is written
    if (pImage == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "Error: Firmware image is null\n");
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto pciBdfInfo = pLinuxSysmanImp->getPciBdfInfo();
    if (pciBdfInfo == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "Error: Could not get the pci bdf info of the device\n");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // The MTD device name holds the id of the auxiliary device created by the KMD, which is derived
    // from the PCI BDF, excluding the domain. Format each BDF component as hex, concatenate, then
    // parse as hex to get decimal. For example 0000:03:00.0 leads to the name xe.nvm.768.DATA
    std::ostringstream bdfHexStream;
    bdfHexStream << std::hex << pciBdfInfo->pciBus << pciBdfInfo->pciDevice << pciBdfInfo->pciFunction;
    uint64_t bdfValue = std::stoul(bdfHexStream.str(), nullptr, 16);
    std::string deviceBdf = std::to_string(bdfValue);

    std::vector<std::string> mtdLines;
    auto pFsAccess = &pLinuxSysmanImp->getFsAccess();

    ze_result_t result = pFsAccess->read(procMtdPath, mtdLines);
    if (result != ZE_RESULT_SUCCESS || mtdLines.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not read %s, result 0x%x\n", procMtdPath.c_str(), result);
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // The kernel reports the device name in quotes
    const std::string expectedName = "\"" + procMtdStringPrefix + deviceBdf + procMtdStringSuffix + "\"";
    std::string mtdDevicePath;
    uint32_t mtdDeviceSize = 0;

    // Entries are reported as: mtd0: 00800000 00001000 "device_name", after a header line
    for (size_t i = 1; i < mtdLines.size(); i++) {
        std::istringstream lineStream(mtdLines[i]);
        std::string mtdNumber, deviceSize, eraseSize, name;

        if (!(lineStream >> mtdNumber >> deviceSize >> eraseSize >> name)) {
            continue;
        }

        if (name != expectedName) {
            continue;
        }

        std::istringstream deviceSizeStream(deviceSize);
        deviceSizeStream >> std::hex >> mtdDeviceSize;
        if (deviceSizeStream.fail()) {
            continue;
        }

        mtdDevicePath = "/dev/" + mtdNumber.substr(0, mtdNumber.find(':'));
        break;
    }

    if (mtdDevicePath.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Mtd device %s was not found in %s\n", expectedName.c_str(), procMtdPath.c_str());
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    // The image is flashed as-is from the start of the device, hence it cannot exceed the device size
    if (size == 0 || size > mtdDeviceSize) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Firmware image of %u bytes does not fit in mtd device %s of %u bytes\n", size, mtdDevicePath.c_str(), mtdDeviceSize);
        return ZE_RESULT_ERROR_INVALID_SIZE;
    }

    auto pMtdDevice = MemoryTechnologyDeviceInterface::create();

    // The whole device is erased, so that no stale contents are left beyond the image
    result = pMtdDevice->erase(mtdDevicePath, 0, mtdDeviceSize);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not erase %u bytes of mtd device %s, result 0x%x\n", mtdDeviceSize, mtdDevicePath.c_str(), result);
        return result;
    }

    result = pMtdDevice->write(mtdDevicePath, 0, static_cast<const uint8_t *>(pImage), size);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error: Could not write %u bytes to mtd device %s, result 0x%x\n", size, mtdDevicePath.c_str(), result);
    }

    return result;
}

ze_result_t LinuxFirmwareImp::osGetSecurityVersion(char *pVersion) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osSetSecurityVersion() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osGetConsoleLogs(size_t *pSize, char *pFirmwareLog) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t LinuxFirmwareImp::osGetFirmwareFlashProgress(uint32_t *pCompletionPercent) {
    if (pSysmanKmdInterface->isDeviceInFdoMode() || (osFwType == fdoFwType)) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    }
    return pFwInterface->getFlashFirmwareProgress(pCompletionPercent);
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

void LinuxFirmwareImp::reInit() {
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<LinuxFirmwareImp> pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, fwType);
    return pLinuxFirmwareImp;
}

} // namespace Sysman
} // namespace L0
