/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <cstdint>
namespace OCLRT {
struct HardwareInfo;

namespace DeviceHelper {
void getExtraDeviceInfo(cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize);
uint32_t getDevicesCount(const HardwareInfo *pHwInfo);
}; // namespace DeviceHelper
} // namespace OCLRT
