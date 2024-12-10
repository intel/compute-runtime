/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"

#include <time.h>

namespace NEO {

DeviceTimeDrm::DeviceTimeDrm(OSInterface &osInterface) {
    pDrm = osInterface.getDriverModel()->as<Drm>();
}

TimeQueryStatus DeviceTimeDrm::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {

    if (!pDrm->getIoctlHelper()->setGpuCpuTimes(pGpuCpuTime, osTime)) {
        if (pDrm->getErrno() == EOPNOTSUPP) {
            return TimeQueryStatus::unsupportedFeature;
        } else {
            return TimeQueryStatus::deviceLost;
        }
    }

    return TimeQueryStatus::success;
}

double DeviceTimeDrm::getDynamicDeviceTimerResolution() const {
    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return nanosecondsPerSecond / frequency;
        }
    }
    return OSTime::getDeviceTimerResolution();
}

uint64_t DeviceTimeDrm::getDynamicDeviceTimerClock() const {

    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return static_cast<uint64_t>(frequency);
        }
    }
    return static_cast<uint64_t>(nanosecondsPerSecond / OSTime::getDeviceTimerResolution());
}

bool DeviceTimeDrm::isTimestampsRefreshEnabled() const {
    bool timestampsRefreshEnabled = pDrm->getIoctlHelper()->isTimestampsRefreshEnabled();
    if (debugManager.flags.EnableReusingGpuTimestamps.get() != -1) {
        timestampsRefreshEnabled = debugManager.flags.EnableReusingGpuTimestamps.get();
    }
    return timestampsRefreshEnabled;
}

} // namespace NEO
