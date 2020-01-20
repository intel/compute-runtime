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
struct HardwareInfo;

namespace DeviceHelper {
void getExtraDeviceInfo(const HardwareInfo &hwInfo, cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize);
}; // namespace DeviceHelper
} // namespace NEO
