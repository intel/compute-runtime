/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device_info_map.h"

namespace NEO {
namespace LEO {

template <cl_device_info param>
inline void ClDevice::getCap(const void *&src,
                             size_t &size,
                             size_t &retSize) {
    src = &ClDeviceInfoTable::Map<param>::getValue(*this);
    retSize = size = ClDeviceInfoTable::Map<param>::size;
}

} // namespace LEO
} // namespace NEO
