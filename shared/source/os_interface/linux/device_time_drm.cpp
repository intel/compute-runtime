/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"

#include "shared/source/helpers/register_offsets.h"
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

    reg.offset = (REG_GLOBAL_TIMESTAMP_LDW | 1);
    err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
    if (err) {
        reg.offset = REG_GLOBAL_TIMESTAMP_UN;
        err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
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
    struct drm_i915_reg_read reg = {};

    reg.offset = REG_GLOBAL_TIMESTAMP_LDW;

    if (pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg)) {
        return false;
    }
    *timestamp = reg.val >> 32;
    return true;
}

bool DeviceTimeDrm::getGpuTime36(uint64_t *timestamp) {
    struct drm_i915_reg_read reg = {};

    reg.offset = REG_GLOBAL_TIMESTAMP_LDW | 1;

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

    reg_hi.offset = REG_GLOBAL_TIMESTAMP_UN;
    reg_lo.offset = REG_GLOBAL_TIMESTAMP_LDW;

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
