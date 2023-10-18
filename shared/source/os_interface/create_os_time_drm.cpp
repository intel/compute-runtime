/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_time.h"

namespace NEO {

std::unique_ptr<OSTime> OSTime::create(OSInterface *osInterface) {
    if (osInterface) {
        return OSTimeLinux::create(*osInterface, std::make_unique<DeviceTimeDrm>(*osInterface));
    }

    return std::make_unique<OSTime>(std::make_unique<DeviceTime>());
}

} // namespace NEO
