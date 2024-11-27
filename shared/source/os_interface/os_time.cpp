/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"

#include <mutex>

namespace NEO {

double OSTime::getDeviceTimerResolution() {
    return CommonConstants::defaultProfilingTimerResolution;
};

TimeQueryStatus DeviceTime::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    pGpuCpuTime->cpuTimeinNS = 0;
    pGpuCpuTime->gpuTimeStamp = 0;

    return TimeQueryStatus::success;
}
double DeviceTime::getDynamicDeviceTimerResolution() const {
    return OSTime::getDeviceTimerResolution();
}

uint64_t DeviceTime::getDynamicDeviceTimerClock() const {
    return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution());
}

void DeviceTime::setDeviceTimerResolution() {
    deviceTimerResolution = getDynamicDeviceTimerResolution();
    if (debugManager.flags.OverrideProfilingTimerResolution.get() != -1) {
        deviceTimerResolution = static_cast<double>(debugManager.flags.OverrideProfilingTimerResolution.get());
    }
}

bool DeviceTime::isTimestampsRefreshEnabled() const {
    bool timestampsRefreshEnabled = true;
    if (debugManager.flags.EnableReusingGpuTimestamps.get() != -1) {
        timestampsRefreshEnabled = debugManager.flags.EnableReusingGpuTimestamps.get();
    }
    return timestampsRefreshEnabled;
}

/**
 * @brief If this method is called within interval, GPU timestamp
 * will be calculated based on CPU timestamp and previous GPU ticks
 * to reduce amount of internal KMD calls. Interval is selected
 * adaptively, based on misalignment between calculated ticks and actual ticks.
 *
 * @return returns appropriate error if internal call to KMD failed. SUCCESS otherwise.
 */
TimeQueryStatus DeviceTime::getGpuCpuTimestamps(TimeStampData *timeStamp, OSTime *osTime, bool forceKmdCall) {
    uint64_t cpuTimeinNS;
    osTime->getCpuTime(&cpuTimeinNS);

    auto cpuTimeDiffInNS = cpuTimeinNS - fetchedTimestamps.cpuTimeinNS;
    if (forceKmdCall || cpuTimeDiffInNS >= timestampRefreshTimeoutNS) {
        refreshTimestamps = true;
    }
    bool reusingTimestampsEnabled = isTimestampsRefreshEnabled();
    if (!reusingTimestampsEnabled || refreshTimestamps) {
        TimeQueryStatus retVal = getGpuCpuTimeImpl(timeStamp, osTime);
        if (retVal != TimeQueryStatus::success) {
            return retVal;
        }
        if (!reusingTimestampsEnabled) {
            return TimeQueryStatus::success;
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

    return TimeQueryStatus::success;
}

TimeQueryStatus DeviceTime::getGpuCpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime, bool forceKmdCall) {
    TimeQueryStatus retVal = getGpuCpuTimestamps(pGpuCpuTime, osTime, forceKmdCall);
    if (retVal != TimeQueryStatus::success) {
        return retVal;
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
    return retVal;
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
