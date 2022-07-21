/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/extensions/public/cl_ext_private.h"

#include "CL/cl.h"

#include <cstdint>
namespace NEO {
class ClDevice;
struct ClDeviceInfoParam;
struct HardwareInfo;

namespace ClDeviceHelper {
void getExtraDeviceInfo(const ClDevice &clDevice, cl_device_info paramName, ClDeviceInfoParam &param, const void *&src, size_t &size, size_t &retSize);
}; // namespace ClDeviceHelper
} // namespace NEO
