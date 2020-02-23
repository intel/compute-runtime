/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/sys_calls.h"

namespace NEO {

HwDeviceId::~HwDeviceId() {
    SysCalls::close(fileDescriptor);
}

} // namespace NEO
