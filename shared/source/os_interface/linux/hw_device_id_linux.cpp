/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/sys_calls.h"

namespace NEO {

HwDeviceIdDrm::~HwDeviceIdDrm() {
    if (this->fileDescriptor >= 0) {
        SysCalls::close(this->fileDescriptor);
    }
}

} // namespace NEO
