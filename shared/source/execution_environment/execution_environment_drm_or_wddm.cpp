/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include <fcntl.h>
#include <unistd.h>

namespace NEO {

void ExecutionEnvironment::adjustRootDeviceEnvironments() {
    if (!rootDeviceEnvironments.empty() && rootDeviceEnvironments[0]->osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm) {
        for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceEnvironments.size(); rootDeviceIndex++) {
            auto drmMemoryOperationsHandler = static_cast<DrmMemoryOperationsHandler *>(rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
            drmMemoryOperationsHandler->setRootDeviceIndex(rootDeviceIndex);
        }
    }
}

void ExecutionEnvironment::configureCcsMode() {
    const auto &ccsString = debugManager.flags.ZEX_NUMBER_OF_CCS.get();
    if (ccsString.compare("default") == 0 ||
        ccsString.empty()) {
        return;
    }

    if (rootDeviceEnvironments.empty() || rootDeviceEnvironments[0]->osInterface->getDriverModel()->getDriverModelType() != DriverModelType::drm) {
        return;
    }

    char *endPtr = nullptr;
    uint32_t ccsMode = static_cast<uint32_t>(std::strtoul(ccsString.c_str(), &endPtr, 10));
    if (endPtr == ccsString.c_str()) {
        return;
    }

    const std::string drmPath = "/sys/class/drm";
    const std::string expectedFilePrefix = drmPath + "/card";

    auto drm = rootDeviceEnvironments[0]->osInterface->getDriverModel()->as<NEO::Drm>();
    auto ioctlHelper = drm->getIoctlHelper();
    auto files = Directory::getFiles(drmPath.c_str());
    ioctlHelper->configureCcsMode(files, expectedFilePrefix, ccsMode, deviceCcsModeVec);
}

void ExecutionEnvironment::restoreCcsMode() {
    for (auto &[ccsFile, ccsMode] : deviceCcsModeVec) {
        auto fd = FileDescriptor(ccsFile.c_str(), O_WRONLY);
        DEBUG_BREAK_IF(fd < 0);
        if (fd > 0) {
            [[maybe_unused]] auto ret = SysCalls::write(fd, &ccsMode, sizeof(uint32_t));
            DEBUG_BREAK_IF(ret < 0);
        }
    }
    deviceCcsModeVec.clear();
}

} // namespace NEO
