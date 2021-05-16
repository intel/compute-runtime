/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>
namespace NEO {
class ClDevice;
struct ClDeviceInfoParam;

namespace ClDeviceHelper {
void getExtraDeviceInfo(const ClDevice &clDevice, cl_device_info paramName, ClDeviceInfoParam &param, const void *&src, size_t &size, size_t &retSize);
}; // namespace ClDeviceHelper
} // namespace NEO
