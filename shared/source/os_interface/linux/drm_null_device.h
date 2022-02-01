/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "drm/i915_drm.h"

#include <cstdio>

namespace NEO {

class DrmNullDevice : public Drm {

  public:
    int ioctl(unsigned long request, void *arg) override {
        if (request == DRM_IOCTL_I915_GETPARAM || request == DRM_IOCTL_I915_QUERY) {
            return Drm::ioctl(request, arg);
        } else if (request == DRM_IOCTL_I915_REG_READ) {
            struct drm_i915_reg_read *regArg = static_cast<struct drm_i915_reg_read *>(arg);

            // Handle only 36b timestamp
            if (regArg->offset == (REG_GLOBAL_TIMESTAMP_LDW | 1)) {
                gpuTimestamp += 1000;
                regArg->val = gpuTimestamp & 0x0000000FFFFFFFFF;
            } else if (regArg->offset == REG_GLOBAL_TIMESTAMP_LDW || regArg->offset == REG_GLOBAL_TIMESTAMP_UN) {
                return -1;
            }

            return 0;
        } else {
            return 0;
        }
    }

    DrmNullDevice(std::unique_ptr<HwDeviceIdDrm> &&hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::move(hwDeviceId), rootDeviceEnvironment), gpuTimestamp(0){};

  protected:
    uint64_t gpuTimestamp;
};
} // namespace NEO
