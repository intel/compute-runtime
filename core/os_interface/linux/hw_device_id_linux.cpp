/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/os_interface/linux/hw_device_id.h"
#include "core/os_interface/linux/sys_calls.h"

namespace NEO {

HwDeviceId::~HwDeviceId() {
    SysCalls::close(fileDescriptor);
}

} // namespace NEO
