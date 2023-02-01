/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"

#include "shared/source/helpers/hw_info.h"

namespace NEO {

double OSTime::getDeviceTimerResolution(HardwareInfo const &hwInfo) {
    return hwInfo.capabilityTable.defaultProfilingTimerResolution;
};

bool DeviceTime::getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    pGpuCpuTime->CPUTimeinNS = 0;
    pGpuCpuTime->GPUTimeStamp = 0;

    return true;
}
double DeviceTime::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    return OSTime::getDeviceTimerResolution(hwInfo);
}

uint64_t DeviceTime::getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
    return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
}

bool OSTime::getCpuTime(uint64_t *timeStamp) {
    *timeStamp = 0;
    return true;
}

double OSTime::getHostTimerResolution() const {
    return 0;
}

uint64_t OSTime::getCpuRawTimestamp() {
    return 0;
}

OSTime::OSTime(std::unique_ptr<DeviceTime> deviceTime) {
    this->deviceTime = std::move(deviceTime);
}
} // namespace NEO
