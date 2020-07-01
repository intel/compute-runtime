/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/pmt.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>

namespace L0 {
const std::string PlatformMonitoringTech::baseTelemDevice("/dev/telem");
const std::string PlatformMonitoringTech::baseTelemSysFS("/sys/class/pmt_telem/telem");

void PlatformMonitoringTech::init(const std::string &deviceName, FsAccess *pFsAccess) {
    pmtSupported = false;

    std::string deviceNumber("1"); // Temporarily hardcoded
    std::string telemetryDeviceEntry = baseTelemDevice + deviceNumber;
    if (!pFsAccess->fileExists(telemetryDeviceEntry)) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry support not available. No file %s\n", telemetryDeviceEntry.c_str());
        return;
    }

    std::string guid;
    std::string guidPath = baseTelemSysFS + deviceNumber + std::string("/guid");
    ze_result_t result = pFsAccess->read(guidPath, guid);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", guidPath.c_str());
        return;
    }

    std::string sizePath = baseTelemSysFS + deviceNumber + std::string("/size");
    result = pFsAccess->read(sizePath, size);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", sizePath.c_str());
        return;
    }

    std::string offsetPath = baseTelemSysFS + deviceNumber + std::string("/offset");
    result = pFsAccess->read(offsetPath, baseOffset);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Telemetry sysfs entry not available %s\n", offsetPath.c_str());
        return;
    }

    int fd = open(static_cast<const char *>(telemetryDeviceEntry.c_str()), O_RDONLY);
    if (fd == -1) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure opening telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        return;
    }

    mappedMemory = static_cast<char *>(mmap(nullptr, static_cast<size_t>(size), PROT_READ, MAP_SHARED, fd, 0));
    if (mappedMemory == MAP_FAILED) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure mapping telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        close(fd);
        return;
    }

    if (close(fd) == -1) {
        NEO::printDebugString(NEO::DebugManager.flags.PrintDebugMessages.get(), stderr,
                              "Failure closing telemetry file %s : %s \n", telemetryDeviceEntry.c_str(), strerror(errno));
        munmap(mappedMemory, size);
        return;
    }

    mappedMemory += baseOffset;
    pmtSupported = true;
}

PlatformMonitoringTech::~PlatformMonitoringTech() {
    if (mappedMemory != nullptr) {
        munmap(mappedMemory - baseOffset, size);
    }
}

} // namespace L0
