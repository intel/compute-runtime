/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device_info.h"

namespace NEO {
namespace LEO {

bool ClDevice::getDeviceInfoExtra(cl_device_info paramName,
                                  ClDeviceInfoParam &param,
                                  const void *&src,
                                  size_t &srcSize,
                                  size_t &retSize) {
    return false;
}

} // namespace LEO
}; // namespace NEO
