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
