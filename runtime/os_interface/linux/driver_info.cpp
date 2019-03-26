/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/driver_info.h"

namespace NEO {

DriverInfo *DriverInfo::create(OSInterface *osInterface) {
    return new DriverInfo();
};
} // namespace NEO