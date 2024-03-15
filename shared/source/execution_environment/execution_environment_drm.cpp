/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/utilities/directory.h"

#include <fcntl.h>
#include <unistd.h>
namespace NEO {

void ExecutionEnvironment::adjustRootDeviceEnvironments() {
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < rootDeviceEnvironments.size(); rootDeviceIndex++) {
        auto drmMemoryOperationsHandler = static_cast<DrmMemoryOperationsHandler *>(rootDeviceEnvironments[rootDeviceIndex]->memoryOperationsInterface.get());
        drmMemoryOperationsHandler->setRootDeviceIndex(rootDeviceIndex);
    }
}

void ExecutionEnvironment::configureCcsMode() {
    const auto &ccsString = debugManager.flags.ZEX_NUMBER_OF_CCS.get();

    if (ccsString.compare("default") == 0 ||
        ccsString.empty()) {
        return;
    }

    char *endPtr = nullptr;
    uint32_t ccsMode = static_cast<uint32_t>(std::strtoul(ccsString.c_str(), &endPtr, 10));
    if (endPtr == ccsString.c_str()) {
        return;
    }

    const std::string drmPath = "/sys/class/drm";
    std::string expectedFilePrefix = drmPath + "/card";
    auto files = Directory::getFiles(drmPath.c_str());
    for (const auto &file : files) {
        if (file.find(expectedFilePrefix.c_str()) == std::string::npos) {
            continue;
        }

        std::string gtPath = file + "/gt";
        auto gtFiles = Directory::getFiles(gtPath.c_str());
        expectedFilePrefix = gtPath + "/gt";
        for (const auto &gtFile : gtFiles) {
            if (gtFile.find(expectedFilePrefix.c_str()) == std::string::npos) {
                continue;
            }
            std::string ccsFile = gtFile + "/ccs_mode";
            auto fd = FileDescriptor(ccsFile.c_str(), O_RDWR);
            if (fd < 0) {
                if ((errno == -EACCES) || (errno == -EPERM)) {
                    fprintf(stderr, "No read and write permissions for %s, System administrator needs to grant permissions to allow modification of this file from user space\n", ccsFile.c_str());
                    fprintf(stdout, "No read and write permissions for %s, System administrator needs to grant permissions to allow modification of this file from user space\n", ccsFile.c_str());
                }
                continue;
            }

            uint32_t ccsValue = 0;
            ssize_t ret = SysCalls::read(fd, &ccsValue, sizeof(uint32_t));
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get() && (ret < 0), stderr, "read() on %s failed errno = %d | ret = %d \n",
                               ccsFile.c_str(), errno, ret);

            if ((ret < 0) || (ccsValue == ccsMode)) {
                continue;
            }

            do {
                ret = SysCalls::write(fd, &ccsMode, sizeof(uint32_t));
            } while (ret == -1 && errno == -EBUSY);

            if (ret > 0) {
                deviceCcsModeVec.emplace_back(ccsFile, ccsValue);
            }
        }
    }
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
