/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/os_interface_linux.h"
#include "shared/source/os_interface/windows/os_interface_win.h"

namespace NEO {

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) {
    if (hwDeviceId->getDriverModelType() == DriverModelType::DRM) {
        return initDrmOsInterface(std::move(hwDeviceId), rootDeviceIndex, this, osInterface, memoryOperationsInterface);
    } else {
        return initWddmOsInterface(std::move(hwDeviceId), rootDeviceIndex, this, osInterface, memoryOperationsInterface);
    }
}

} // namespace NEO
