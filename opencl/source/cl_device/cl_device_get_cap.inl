/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/cl_device/cl_device_info_map.h"

namespace NEO {

template <cl_device_info param>
inline void ClDevice::getCap(const void *&src,
                             size_t &size,
                             size_t &retSize) {
    src = &ClDeviceInfoTable::Map<param>::getValue(*this);
    retSize = size = ClDeviceInfoTable::Map<param>::size;
}

} // namespace NEO
