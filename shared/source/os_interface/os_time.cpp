/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"

#include "shared/source/helpers/hw_info.h"

#include <mutex>

namespace NEO {

double OSTime::getDeviceTimerResolution(HardwareInfo const &hwInfo) {
    return hwInfo.capabilityTable.defaultProfilingTimerResolution;
};

bool DeviceTime::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    pGpuCpuTime->cpuTimeinNS = 0;
    pGpuCpuTime->gpuTimeStamp = 0;

    return true;
}
double DeviceTime::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    return OSTime::getDeviceTimerResolution(hwInfo);
}

uint64_t DeviceTime::getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
    return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
}

bool DeviceTime::getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    if (!getGpuCpuTimeImpl(pGpuCpuTime, osTime)) {
        return false;
    }

    auto maxGpuTimeStampValue = osTime->getMaxGpuTimeStamp();

    static std::mutex gpuTimeStampOverflowCounterMutex;
    std::lock_guard<std::mutex> lock(gpuTimeStampOverflowCounterMutex);
    pGpuCpuTime->gpuTimeStamp &= (maxGpuTimeStampValue - 1);
    if (!initialGpuTimeStamp) {
        initialGpuTimeStamp = pGpuCpuTime->gpuTimeStamp;
        waitingForGpuTimeStampOverflow = true;
    } else {
        if (waitingForGpuTimeStampOverflow && pGpuCpuTime->gpuTimeStamp < *initialGpuTimeStamp) {
            gpuTimeStampOverflowCounter++;
            waitingForGpuTimeStampOverflow = false;
        }
        if (!waitingForGpuTimeStampOverflow && pGpuCpuTime->gpuTimeStamp > *initialGpuTimeStamp) {
            waitingForGpuTimeStampOverflow = true;
        }

        pGpuCpuTime->gpuTimeStamp += gpuTimeStampOverflowCounter * maxGpuTimeStampValue;
    }
    return true;
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
