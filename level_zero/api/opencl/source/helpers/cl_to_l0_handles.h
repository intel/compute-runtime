/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include <level_zero/ze_api.h>

#include "CL/cl.h"

namespace NEO {
namespace LEO {

namespace ConvertTo {
ze_device_handle_t zeDeviceHandle(cl_device_id device);
} // namespace ConvertTo

} // namespace LEO
} // namespace NEO
