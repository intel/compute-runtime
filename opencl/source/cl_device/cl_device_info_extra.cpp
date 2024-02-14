/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/cl_device/cl_device_info.h"

namespace NEO {
bool ClDevice::getDeviceInfoExtra(cl_device_info paramName,
                                  ClDeviceInfoParam &param,
                                  const void *&src,
                                  size_t &srcSize,
                                  size_t &retSize) {
    return false;
}
}; // namespace NEO