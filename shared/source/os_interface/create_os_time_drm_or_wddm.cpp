/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"
#include "shared/source/os_interface/linux/os_time_linux.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/windows/device_time_wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

std::unique_ptr<OSTime> OSTime::create(OSInterface *osInterface) {
    if (nullptr == osInterface) {
        return std::make_unique<OSTime>(std::make_unique<DeviceTime>());
    } else if (osInterface->getDriverModel()->getDriverModelType() == DriverModelType::drm) {
        return OSTimeLinux::create(*osInterface, std::make_unique<DeviceTimeDrm>(*osInterface));
    } else {
        auto wddm = osInterface->getDriverModel()->as<Wddm>();
        return OSTimeLinux::create(*osInterface, std::make_unique<DeviceTimeWddm>(wddm));
    }
}

} // namespace NEO
