/*
 * Copyright (C) 2017-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "drm/i915_drm.h"

#include <time.h>

namespace NEO {

DeviceTimeDrm::DeviceTimeDrm(OSInterface *osInterface) {
    if (osInterface) {
        pDrm = osInterface->getDriverModel()->as<Drm>();
    }
    timestampTypeDetect();
}

void DeviceTimeDrm::timestampTypeDetect() {
    struct drm_i915_reg_read reg = {};
    int err;

    if (pDrm == nullptr)
        return;

    reg.offset = (TIMESTAMP_LOW_REG | 1);
    err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
    if (err) {
        reg.offset = TIMESTAMP_HIGH_REG;
        err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
        if (err) {
            getGpuTime = &DeviceTimeDrm::getGpuTime32;
            timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS_FALLBACK;
        } else {
            getGpuTime = &DeviceTimeDrm::getGpuTimeSplitted;
            timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS;
        }
    } else {
        getGpuTime = &DeviceTimeDrm::getGpuTime36;
        timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS;
    }
}

bool DeviceTimeDrm::getGpuTime32(uint64_t *timestamp) {
    struct drm_i915_reg_read reg = {};

    reg.offset = TIMESTAMP_LOW_REG;

    if (pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg)) {
        return false;
    }
    *timestamp = reg.val >> 32;
    return true;
}

bool DeviceTimeDrm::getGpuTime36(uint64_t *timestamp) {
    struct drm_i915_reg_read reg = {};

    reg.offset = TIMESTAMP_LOW_REG | 1;

    if (pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg)) {
        return false;
    }
    *timestamp = reg.val;
    return true;
}

bool DeviceTimeDrm::getGpuTimeSplitted(uint64_t *timestamp) {
    struct drm_i915_reg_read reg_hi = {};
    struct drm_i915_reg_read reg_lo = {};
    uint64_t tmp_hi;
    int err = 0, loop = 3;

    reg_hi.offset = TIMESTAMP_HIGH_REG;
    reg_lo.offset = TIMESTAMP_LOW_REG;

    err += pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg_hi);
    do {
        tmp_hi = reg_hi.val;
        err += pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg_lo);
        err += pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg_hi);
    } while (err == 0 && reg_hi.val != tmp_hi && --loop);

    if (err) {
        return false;
    }

    *timestamp = reg_lo.val | (reg_hi.val << 32);
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
