/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/os_time_linux.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/os_interface.h"

#include <chrono>
#include <time.h>

namespace NEO {

OSTimeLinux::OSTimeLinux(OSInterface &osInterface, std::unique_ptr<DeviceTime> deviceTime) {
    this->osInterface = &osInterface;
    auto hwInfo = osInterface.getDriverModel()->getHardwareInfo();
    if (hwInfo->capabilityTable.timestampValidBits < 64) {
        this->maxGpuTimeStamp = 1ull << hwInfo->capabilityTable.timestampValidBits;
    }

    resolutionFunc = &clock_getres;
    getTimeFunc = &clock_gettime;
    this->deviceTime = std::move(deviceTime);
}

bool OSTimeLinux::getCpuTime(uint64_t *timestamp) {
    struct timespec ts {
        0, 0
    };

    if (getTimeFunc(CLOCK_MONOTONIC_RAW, &ts)) {
        return false;
    }

    *timestamp = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

    return true;
}

bool OSTime::getCpuTimeHost(uint64_t *timestamp) {
    struct timespec ts {
        0, 0
    };

    auto ret = clock_gettime(CLOCK_MONOTONIC_RAW, &ts);

    *timestamp = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

    return ret;
}

double OSTimeLinux::getHostTimerResolution() const {
    struct timespec ts {
        0, 0
    };
    if (resolutionFunc(CLOCK_MONOTONIC_RAW, &ts)) {
        return 0;
    }
    return static_cast<double>(ts.tv_nsec + ts.tv_sec * NSEC_PER_SEC);
}

uint64_t OSTimeLinux::getCpuRawTimestamp() {
    uint64_t timesInNsec = 0;
    uint64_t ticksInNsec = 0;
    if (!getCpuTime(&timesInNsec)) {
        return 0;
    }
    ticksInNsec = static_cast<uint64_t>(getHostTimerResolution());
    if (ticksInNsec == 0) {
        return 0;
    }
    return timesInNsec / ticksInNsec;
}

std::unique_ptr<OSTime> OSTimeLinux::create(OSInterface &osInterface, std::unique_ptr<DeviceTime> deviceTime) {
    return std::unique_ptr<OSTime>(new OSTimeLinux(osInterface, std::move(deviceTime)));
}

} // namespace NEO
