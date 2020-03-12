/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/device/cl_device.h"
#include "opencl/source/device/device_info_map.h"

namespace NEO {

template <cl_device_info Param>
inline void ClDevice::getCap(const void *&src,
                             size_t &size,
                             size_t &retSize) {
    src = &DeviceInfoTable::Map<Param>::getValue(*this);
    retSize = size = DeviceInfoTable::Map<Param>::size;
}

} // namespace NEO
