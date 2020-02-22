/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/device/driver_info.h"

namespace NEO {

DriverInfo *DriverInfo::create(OSInterface *osInterface) {
    return new DriverInfo();
};
} // namespace NEO