/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/helpers/cl_to_l0_handles.h"

namespace NEO {
namespace LEO {

namespace ConvertTo {
ze_device_handle_t zeDeviceHandle(cl_device_id device) {
    auto clDevice = NEO::LEO::castToObject<NEO::LEO::ClDevice>(device);
    if (!clDevice) {
        return nullptr;
    }
    return clDevice->getL0Handle();
}
} // namespace ConvertTo

} // namespace LEO
} // namespace NEO
