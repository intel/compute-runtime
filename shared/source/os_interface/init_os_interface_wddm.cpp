/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/os_interface_win.h"

namespace NEO {

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) {
    return initWddmOsInterface(std::move(hwDeviceId), rootDeviceIndex, this, osInterface, memoryOperationsInterface);
}

} // namespace NEO
