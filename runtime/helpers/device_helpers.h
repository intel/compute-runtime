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
void getExtraDeviceInfo(cl_device_info paramName, cl_uint &param, const void *&src, size_t &size, size_t &retSize);
}
