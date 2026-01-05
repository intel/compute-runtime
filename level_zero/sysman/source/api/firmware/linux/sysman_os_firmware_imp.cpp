/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"

#include "shared/source/helpers/string.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/mtd/sysman_mtd.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace L0 {
namespace Sysman {

static const std::string fdoFwType = "Flash_Override";
static const std::string procMtdPath = "/proc/mtd";
static const std::string procMtdStringPrefix = "xe.nvm.";

void OsFirmware::getSupportedFwTypes(std::vector<std::string> &supportedFwTypes, OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    supportedFwTypes.clear();
    bool isDeviceInSurvivabilityMode = pLinuxSysmanImp->isDeviceInSurvivabilityMode();

    if (isDeviceInSurvivabilityMode && pSysmanKmdInterface->isDeviceInFdoMode()) {
        supportedFwTypes.push_back(fdoFwType);
        return;
    }

    if (pFwInterface != nullptr) {
        if (isDeviceInSurvivabilityMode) {
            pFwInterface->getDeviceSupportedFwTypes(supportedFwTypes);
        } else {
            auto pSysmanProductHelper = pLinuxSysmanImp->getSysmanProductHelper();
            pSysmanProductHelper->getDeviceSupportedFwTypes(pFwInterface, supportedFwTypes);
            // get supported late binding fw handles
            pSysmanKmdInterface->getLateBindingSupportedFwTypes(supportedFwTypes);
        }
    }
}

void LinuxFirmwareImp::osGetFwProperties(zes_firmware_properties_t *pProperties) {
    if (ZE_RESULT_SUCCESS != getFirmwareVersion(osFwType, pProperties)) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, unknown.data(), ZES_STRING_PROPERTY_SIZE - 1);
    }
    pProperties->canControl = true; // Assuming that user has permission to flash the firmware
}

ze_result_t LinuxFirmwareImp::osFirmwareFlash(void *pImage, uint32_t size) {
    if (osFwType == fdoFwType) {
        return osFirmwareFlashExtended(pImage, size);
    }
    return pFwInterface->flashFirmware(osFwType, pImage, size);
}

ze_result_t LinuxFirmwareImp::osFirmwareFlashExtended(void *pImage, uint32_t size) {
    // Get the current device's PCI BDF
    auto pciBdfInfo = pLinuxSysmanImp->getPciBdfInfo();

    // Format the BDF string as "busdevicefunction"
    std::string deviceBdf = std::to_string(pciBdfInfo->pciBus) +
                            std::to_string(pciBdfInfo->pciDevice) +
                            std::to_string(pciBdfInfo->pciFunction);

    // Read /proc/mtd to find matching MTD devices
    std::vector<std::string> mtdLines;
    auto pFsAccess = &pLinuxSysmanImp->getFsAccess();

    ze_result_t result = pFsAccess->read(procMtdPath, mtdLines);
    if (result != ZE_RESULT_SUCCESS || mtdLines.empty()) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::map<std::string, std::string> regionToDevicePathMap;
    std::map<std::string, std::map<uint32_t, uint32_t>> mtdRegionDeviceInfoMap;
    bool foundDescriptorDevice = false;

    // Create MTD device interface and perform the flash operation
    auto pMtdDevice = MemoryTechnologyDeviceInterface::create();

    // Skip the header line and parse each MTD device entry
    for (size_t i = 1; i < mtdLines.size(); ++i) {
        const std::string &line = mtdLines[i];
        std::istringstream iss(line);
        std::string mtdNumber, size, eraseSize, name;

        // Parse the line: mtd0: 00800000 00001000 "device_name"
        if (iss >> mtdNumber >> size >> eraseSize >> name) {
            // Remove quotes from device name if present
            if (name.front() == '"' && name.back() == '"') {
                name = name.substr(1, name.length() - 2);
            }

            // Check if the MTD device name matches procMtdStringPrefix + deviceBdf
            std::string expectedName = procMtdStringPrefix + deviceBdf;
            if (name.find(expectedName) == 0) {
                // Remove expectedName + "." from the name to get the region
                std::string prefixToRemove = expectedName + ".";
                std::string region;
                if (name.length() > prefixToRemove.length()) {
                    region = name.substr(prefixToRemove.length());

                    // Remove the colon from mtdNumber (e.g., "mtd0:" -> "mtd0")
                    if (mtdNumber.back() == ':') {
                        mtdNumber.pop_back();
                    }

                    std::string devicePath = "/dev/" + mtdNumber;

                    // Add the region and device path to the map
                    regionToDevicePathMap[region] = devicePath;

                    if (region == "DESCRIPTOR") {
                        result = pMtdDevice->getDeviceInfo(devicePath, mtdRegionDeviceInfoMap);
                        if (result != ZE_RESULT_SUCCESS) {
                            return result;
                        }
                        foundDescriptorDevice = true;
                    }
                }
            }
        }
    }

    if (!foundDescriptorDevice) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    const uint8_t *pImageData = static_cast<const uint8_t *>(pImage);

    // Iterate over each region and perform erase/write operations
    for (const auto &regionEntry : regionToDevicePathMap) {
        const std::string &regionName = regionEntry.first;
        const std::string &mtdPath = regionEntry.second;

        // Check if we have region info for this region
        auto regionInfoIt = mtdRegionDeviceInfoMap.find(regionName);
        if (regionInfoIt == mtdRegionDeviceInfoMap.end()) {
            continue; // Skip if no region info available
        }

        uint32_t regionBegin = regionInfoIt->second[0];
        uint32_t regionEnd = regionInfoIt->second[1];
        uint32_t regionSize = regionEnd - regionBegin + 1;

        // Check if we have enough data for this region
        if (regionSize > size) {
            break; // Not enough data
        }

        // Erase the region
        result = pMtdDevice->erase(mtdPath, regionBegin, regionSize);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }

        // Write the firmware data to this region
        result = pMtdDevice->write(mtdPath, regionBegin, pImageData + regionBegin, regionSize);
        if (result != ZE_RESULT_SUCCESS) {
            return result;
        }
    }

    return ZE_RESULT_SUCCESS;
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
    return pFwInterface->getFlashFirmwareProgress(pCompletionPercent);
}

LinuxFirmwareImp::LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) : osFwType(fwType) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    if (!pLinuxSysmanImp->isDeviceInSurvivabilityMode()) {
        pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
    }
    pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
}

std::unique_ptr<OsFirmware> OsFirmware::create(OsSysman *pOsSysman, const std::string &fwType) {
    std::unique_ptr<LinuxFirmwareImp> pLinuxFirmwareImp = std::make_unique<LinuxFirmwareImp>(pOsSysman, fwType);
    return pLinuxFirmwareImp;
}

} // namespace Sysman
} // namespace L0
