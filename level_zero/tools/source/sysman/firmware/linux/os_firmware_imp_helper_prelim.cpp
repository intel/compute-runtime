/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"

const std::string iafPath = "device/";
const std::string iafDirectory = "iaf.";
const std::string pscbin_version = "/pscbin_version";

namespace L0 {

ze_result_t LinuxFirmwareImp::getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties) {
    std::string fwVersion;
    if (fwType == "PSC") {
        std::string path;
        path.clear();
        std::vector<std::string> list;
        // scans the directories present in /sys/class/drm/cardX/device/
        ze_result_t result = pSysfsAccess->scanDirEntries(iafPath, list);
        if (ZE_RESULT_SUCCESS != result) {
            // There should be a device directory
            return result;
        }
        for (const auto &entry : list) {
            if (!iafDirectory.compare(entry.substr(0, iafDirectory.length()))) {
                // device/iaf.X/pscbin_version, where X is the hardware slot number
                path = iafPath + entry + pscbin_version;
            }
        }
        if (path.empty()) {
            // This device does not have a PSC Version
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        std::string pscVersion;
        pscVersion.clear();
        result = pSysfsAccess->read(path, pscVersion);
        if (ZE_RESULT_SUCCESS != result) {
            // not able to read PSC version from iaf.x
            return result;
        }
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, pscVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
        return result;
    }
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (result == ZE_RESULT_SUCCESS) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE);
    }

    return result;
}

} // namespace L0