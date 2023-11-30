/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/tools/source/sysman/firmware/linux/os_firmware_imp.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"

const std::string iafPath = "device/";
const std::string iafDirectory = "iaf.";
const std::string pscbinVersion = "/pscbin_version";

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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAcess->scanDirEntries() failed to locate device directory at %s and returning error:0x%x \n", __FUNCTION__, iafPath.c_str(), result);
            return result;
        }
        for (const auto &entry : list) {
            if (!iafDirectory.compare(entry.substr(0, iafDirectory.length()))) {
                // device/iaf.X/pscbin_version, where X is the hardware slot number
                path = iafPath + entry + pscbinVersion;
            }
        }
        if (path.empty()) {
            // This device does not have a PSC Version
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): device does not have a PSC version and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_NOT_AVAILABLE);
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        std::string pscVersion;
        pscVersion.clear();
        result = pSysfsAccess->read(path, pscVersion);
        if (ZE_RESULT_SUCCESS != result) {
            // not able to read PSC version from iaf.x
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): SysfsAccess->read() failed to read %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
            return result;
        }
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, pscVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
        return result;
    }
    ze_result_t result = pFwInterface->getFwVersion(fwType, fwVersion);
    if (result == ZE_RESULT_SUCCESS) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
    }

    return result;
}

} // namespace L0
