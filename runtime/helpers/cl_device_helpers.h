/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>
namespace NEO {
class ClDevice;

namespace ClDeviceHelper {
void getExtraDeviceInfo(const ClDevice &clDevice, cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize);
}; // namespace ClDeviceHelper
} // namespace NEO
