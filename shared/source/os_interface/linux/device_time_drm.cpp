/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"

#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/os_interface.h"

#include <time.h>

namespace NEO {

DeviceTimeDrm::DeviceTimeDrm(OSInterface *osInterface) {
    if (osInterface) {
        pDrm = osInterface->getDriverModel()->as<Drm>();
    }
    timestampTypeDetect();
}

void DeviceTimeDrm::timestampTypeDetect() {
    RegisterRead reg = {};
    int err;

    if (pDrm == nullptr)
        return;

    reg.offset = (REG_GLOBAL_TIMESTAMP_LDW | 1);
    auto ioctlHelper = pDrm->getIoctlHelper();
    err = ioctlHelper->ioctl(DrmIoctl::RegRead, &reg);
    if (err) {
        reg.offset = REG_GLOBAL_TIMESTAMP_UN;
        err = ioctlHelper->ioctl(DrmIoctl::RegRead, &reg);
        if (err) {
            getGpuTime = &DeviceTimeDrm::getGpuTime32;
        } else {
            getGpuTime = &DeviceTimeDrm::getGpuTimeSplitted;
        }
    } else {
        getGpuTime = &DeviceTimeDrm::getGpuTime36;
    }
}

bool DeviceTimeDrm::getGpuTime32(uint64_t *timestamp) {
    RegisterRead reg = {};

    reg.offset = REG_GLOBAL_TIMESTAMP_LDW;

    auto ioctlHelper = pDrm->getIoctlHelper();
    if (ioctlHelper->ioctl(DrmIoctl::RegRead, &reg)) {
        return false;
    }
    *timestamp = reg.value >> 32;
    return true;
}

bool DeviceTimeDrm::getGpuTime36(uint64_t *timestamp) {
    RegisterRead reg = {};

    reg.offset = REG_GLOBAL_TIMESTAMP_LDW | 1;

    auto ioctlHelper = pDrm->getIoctlHelper();
    if (ioctlHelper->ioctl(DrmIoctl::RegRead, &reg)) {
        return false;
    }
    *timestamp = reg.value;
    return true;
}

bool DeviceTimeDrm::getGpuTimeSplitted(uint64_t *timestamp) {
    RegisterRead regHi = {};
    RegisterRead regLo = {};
    uint64_t tmpHi;
    int err = 0, loop = 3;

    regHi.offset = REG_GLOBAL_TIMESTAMP_UN;
    regLo.offset = REG_GLOBAL_TIMESTAMP_LDW;

    auto ioctlHelper = pDrm->getIoctlHelper();
    err += ioctlHelper->ioctl(DrmIoctl::RegRead, &regHi);
    do {
        tmpHi = regHi.value;
        err += ioctlHelper->ioctl(DrmIoctl::RegRead, &regLo);
        err += ioctlHelper->ioctl(DrmIoctl::RegRead, &regHi);
    } while (err == 0 && regHi.value != tmpHi && --loop);

    if (err) {
        return false;
    }

    *timestamp = regLo.value | (regHi.value << 32);
    return true;
}

bool DeviceTimeDrm::getCpuGpuTime(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    if (nullptr == this->getGpuTime) {
        return false;
    }
    if (!(this->*getGpuTime)(&pGpuCpuTime->GPUTimeStamp)) {
        return false;
    }
    if (!osTime->getCpuTime(&pGpuCpuTime->CPUTimeinNS)) {
        return false;
    }

    return true;
}

double DeviceTimeDrm::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return 1000000000.0 / frequency;
        }
    }
    return OSTime::getDeviceTimerResolution(hwInfo);
}

uint64_t DeviceTimeDrm::getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {
    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return static_cast<uint64_t>(frequency);
        }
    }
    return static_cast<uint64_t>(1000000000.0 / OSTime::getDeviceTimerResolution(hwInfo));
}

} // namespace NEO
