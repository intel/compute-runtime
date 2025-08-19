/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"

#include "level_zero/sysman/source/api/firmware/linux/sysman_os_firmware_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

const std::string iafPath = "device/";
const std::string iafDirectory = "iaf.";
const std::string pscbinVersion = "/pscbin_version";

namespace L0 {
namespace Sysman {

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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to scan directories at %s and returning error:0x%x \n", __FUNCTION__, iafPath.c_str(), result);
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
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Failed to read PSC version from iaf.x at %s and returning error:0x%x \n", __FUNCTION__, path.c_str(), result);
            return result;
        }
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, pscVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
        return result;
    }
    if (std::find(lateBindingFirmwareTypes.begin(), lateBindingFirmwareTypes.end(), fwType) != lateBindingFirmwareTypes.end()) {
        if (pLinuxSysmanImp->getSysmanKmdInterface()->isLateBindingVersionAvailable(std::move(fwType), fwVersion)) {
            strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
            return ZE_RESULT_SUCCESS;
        }
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    ze_result_t result = pFwInterface->getFwVersion(std::move(fwType), fwVersion);
    if (result == ZE_RESULT_SUCCESS) {
        strncpy_s(static_cast<char *>(pProperties->version), ZES_STRING_PROPERTY_SIZE, fwVersion.c_str(), ZES_STRING_PROPERTY_SIZE - 1);
    }

    return result;
}

} // namespace Sysman
} // namespace L0
