/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device_imp_drm/device_imp_peer.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/core/source/device/device_imp.h"

#include <fcntl.h>

namespace L0 {

const std::string iafDirectoryLegacy = "iaf.";
const std::string iafDirectory = "i915.iaf.";
const std::string fabricIdFile = "/iaf_fabric_id";

ze_result_t queryFabricStatsDrm(DeviceImp *pSourceDevice, DeviceImp *pPeerDevice, uint32_t &latency, uint32_t &bandwidth) {
    auto &osPeerInterface = pPeerDevice->getNEODevice()->getRootDeviceEnvironment().osInterface;

    if (osPeerInterface == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    auto pPeerDrm = osPeerInterface->getDriverModel()->as<NEO::Drm>();
    auto peerDevicePath = pPeerDrm->getSysFsPciPath();

    std::string fabricPath;
    fabricPath.clear();
    std::vector<std::string> list = NEO::Directory::getFiles(peerDevicePath + "/device");
    for (auto &entry : list) {
        if ((entry.find(iafDirectory) != std::string::npos) || (entry.find(iafDirectoryLegacy) != std::string::npos)) {
            fabricPath = entry + fabricIdFile;
            break;
        }
    }
    if (fabricPath.empty()) {
        // This device does not have a fabric
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    std::string fabricIdStr(64, '\0');
    int fd = NEO::SysCalls::open(fabricPath.c_str(), O_RDONLY);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    NEO::SysCalls::pread(fd, fabricIdStr.data(), fabricIdStr.size() - 1, 0);
    NEO::SysCalls::close(fd);

    size_t end = 0;
    uint32_t fabricId = static_cast<uint32_t>(std::stoul(fabricIdStr, &end, 16));

    auto &osInterface = pSourceDevice->getNEODevice()->getRootDeviceEnvironment().osInterface;
    auto pDrm = osInterface->getDriverModel()->as<NEO::Drm>();
    bool success = pDrm->getIoctlHelper()->getFabricLatency(fabricId, latency, bandwidth);
    if (success == false) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                       "Connection detected between device %d and peer device %d: latency %d hops, bandwidth %d GBPS\n",
                       pSourceDevice->getRootDeviceIndex(), pPeerDevice->getRootDeviceIndex(), latency, bandwidth);

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
