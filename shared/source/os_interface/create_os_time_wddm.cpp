/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/os_time_win.h"

namespace NEO {

std::unique_ptr<OSTime> OSTime::create(OSInterface *osInterface) {
    if (osInterface) {
        return OSTimeWin::create(*osInterface);
    }

    return std::make_unique<OSTime>(std::make_unique<DeviceTime>());
}

} // namespace NEO
