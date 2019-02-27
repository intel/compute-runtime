/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_time_linux.h"

#include "drm/i915_drm.h"

#include <cstdio>

namespace OCLRT {

class DrmNullDevice : public Drm {
    friend Drm;
    friend DeviceFactory;

  public:
    int ioctl(unsigned long request, void *arg) override {
        if (request == DRM_IOCTL_I915_GETPARAM) {
            return Drm::ioctl(request, arg);
        } else if (request == DRM_IOCTL_I915_REG_READ) {
            struct drm_i915_reg_read *regArg = static_cast<struct drm_i915_reg_read *>(arg);

            // Handle only 36b timestamp
            if (regArg->offset == (TIMESTAMP_LOW_REG | 1)) {
                gpuTimestamp += 1000;
                regArg->val = gpuTimestamp & 0x0000000FFFFFFFFF;
            } else if (regArg->offset == TIMESTAMP_LOW_REG || regArg->offset == TIMESTAMP_HIGH_REG) {
                return -1;
            }

            return 0;
        } else {
            return 0;
        }
    }

  protected:
    DrmNullDevice(int fd) : Drm(fd), gpuTimestamp(0){};

    uint64_t gpuTimestamp;
};
} // namespace OCLRT
