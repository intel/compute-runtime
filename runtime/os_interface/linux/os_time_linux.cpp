/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <time.h>
#include "runtime/os_interface/linux/drm_neo.h"
#include "drm/i915_drm.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/linux/os_time_linux.h"

namespace OCLRT {

OSTimeLinux::OSTimeLinux(OSInterface *osInterface) {
    this->osInterface = osInterface;
    resolutionFunc = &clock_getres;
    getTimeFunc = &clock_gettime;
    if (osInterface) {
        pDrm = osInterface->get()->getDrm();
    } else {
        pDrm = Drm::get(0);
    }
    timestampTypeDetect();
}

OSTimeLinux::~OSTimeLinux(){};

void OSTimeLinux::timestampTypeDetect() {
    struct drm_i915_reg_read reg;
    int err;

    if (pDrm == nullptr)
        return;

    reg.offset = (TIMESTAMP_LOW_REG | 1);
    err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
    if (err) {
        reg.offset = TIMESTAMP_HIGH_REG;
        err = pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg);
        if (err) {
            getGpuTime = &OSTimeLinux::getGpuTime32;
            timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS_FALLBACK;
        } else {
            getGpuTime = &OSTimeLinux::getGpuTimeSplitted;
            timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS;
        }
    } else {
        getGpuTime = &OSTimeLinux::getGpuTime36;
        timestampSizeInBits = OCLRT_NUM_TIMESTAMP_BITS;
    }
}

bool OSTimeLinux::getCpuTime(uint64_t *timestamp) {
    struct timespec ts;

    if (getTimeFunc(CLOCK_MONOTONIC_RAW, &ts)) {
        return false;
    }

    *timestamp = (uint64_t)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;

    return true;
}

bool OSTimeLinux::getGpuTime32(uint64_t *timestamp) {
    struct drm_i915_reg_read reg;
    reg.offset = TIMESTAMP_LOW_REG;

    if (pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg)) {
        return false;
    }
    *timestamp = reg.val >> 32;
    return true;
}

bool OSTimeLinux::getGpuTime36(uint64_t *timestamp) {
    struct drm_i915_reg_read reg;
    reg.offset = TIMESTAMP_LOW_REG | 1;

    if (pDrm->ioctl(DRM_IOCTL_I915_REG_READ, &reg)) {
        return false;
    }
    *timestamp = reg.val;
    return true;
}

bool OSTimeLinux::getGpuTimeSplitted(uint64_t *timestamp) {
    struct drm_i915_reg_read reg_hi, reg_lo;
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

bool OSTimeLinux::getCpuGpuTime(TimeStampData *pGpuCpuTime) {
    if (!(this->*getGpuTime)(&pGpuCpuTime->GPUTimeStamp)) {
        return false;
    }
    if (!getCpuTime(&pGpuCpuTime->CPUTimeinNS)) {
        return false;
    }

    return true;
}

std::unique_ptr<OSTime> OSTime::create(OSInterface *osInterface) {
    return std::unique_ptr<OSTime>(new OSTimeLinux(osInterface));
}

double OSTimeLinux::getHostTimerResolution() const {
    struct timespec ts;
    if (resolutionFunc(CLOCK_MONOTONIC_RAW, &ts)) {
        return 0;
    }
    return ts.tv_nsec + ts.tv_sec * NSEC_PER_SEC;
}

double OSTimeLinux::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    return OSTime::getDeviceTimerResolution(hwInfo);
}

uint64_t OSTimeLinux::getCpuRawTimestamp() {
    uint64_t timesInNsec = 0;
    uint64_t ticksInNsec = 0;
    if (!getCpuTime(&timesInNsec)) {
        return 0;
    }
    ticksInNsec = getHostTimerResolution();
    if (ticksInNsec == 0) {
        return 0;
    }
    return timesInNsec / ticksInNsec;
}
} // namespace OCLRT
