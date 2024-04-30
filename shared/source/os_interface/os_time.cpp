/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"

#include <mutex>

namespace NEO {

double OSTime::getDeviceTimerResolution(HardwareInfo const &hwInfo) {
    return hwInfo.capabilityTable.defaultProfilingTimerResolution;
};

DeviceTime::DeviceTime() {
    reusingTimestampsEnabled = debugManager.flags.EnableReusingGpuTimestamps.get();
    if (reusingTimestampsEnabled) {
        timestampRefreshTimeoutNS = NSEC_PER_MSEC * 100; // 100ms
    }
}

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

void DeviceTime::setDeviceTimerResolution(HardwareInfo const &hwInfo) {
    deviceTimerResolution = getDynamicDeviceTimerResolution(hwInfo);
    if (debugManager.flags.OverrideProfilingTimerResolution.get() != -1) {
        deviceTimerResolution = static_cast<double>(debugManager.flags.OverrideProfilingTimerResolution.get());
    }
}

/**
 * @brief If this method is called within interval, GPU timestamp
 * will be calculated based on CPU timestamp and previous GPU ticks
 * to reduce amount of internal KMD calls. Interval is selected
 * adaptively, based on misalignment between calculated ticks and actual ticks.
 *
 * @return returns false if internal call to KMD failed. True otherwise.
 */
bool DeviceTime::getGpuCpuTimestamps(TimeStampData *timeStamp, OSTime *osTime, bool forceKmdCall) {
    uint64_t cpuTimeinNS;
    osTime->getCpuTime(&cpuTimeinNS);

    auto cpuTimeDiffInNS = cpuTimeinNS - fetchedTimestamps.cpuTimeinNS;
    if (forceKmdCall || cpuTimeDiffInNS >= timestampRefreshTimeoutNS) {
        refreshTimestamps = true;
    }

    if (!reusingTimestampsEnabled || refreshTimestamps) {
        if (!getGpuCpuTimeImpl(timeStamp, osTime)) {
            return false;
        }
        if (!reusingTimestampsEnabled) {
            return true;
        }
        if (initialGpuTimeStamp) {
            UNRECOVERABLE_IF(deviceTimerResolution == 0);
            auto calculatedTimestamp = fetchedTimestamps.gpuTimeStamp + static_cast<uint64_t>(cpuTimeDiffInNS / deviceTimerResolution);
            auto diff = abs(static_cast<int64_t>(timeStamp->gpuTimeStamp - calculatedTimestamp));
            auto elapsedTicks = timeStamp->gpuTimeStamp - fetchedTimestamps.gpuTimeStamp;
            int64_t adaptValue = static_cast<int64_t>(diff * deviceTimerResolution);
            adaptValue = std::min(adaptValue, static_cast<int64_t>(timestampRefreshMinTimeoutNS));
            if (diff * 1.0f / elapsedTicks > 0.05) {
                adaptValue = adaptValue * (-1);
            }
            timestampRefreshTimeoutNS += adaptValue;
            timestampRefreshTimeoutNS = std::max(timestampRefreshMinTimeoutNS, std::min(timestampRefreshMaxTimeoutNS, timestampRefreshTimeoutNS));
        }
        fetchedTimestamps = *timeStamp;
        refreshTimestamps = false;
    } else {
        timeStamp->cpuTimeinNS = cpuTimeinNS;
        UNRECOVERABLE_IF(deviceTimerResolution == 0);
        timeStamp->gpuTimeStamp = fetchedTimestamps.gpuTimeStamp + static_cast<uint64_t>(cpuTimeDiffInNS / deviceTimerResolution);
    }

    return true;
}

bool DeviceTime::getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime, bool forceKmdCall) {
    if (!getGpuCpuTimestamps(pGpuCpuTime, osTime, forceKmdCall)) {
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
