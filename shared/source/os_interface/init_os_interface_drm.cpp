/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/linux/os_interface_linux.h"

namespace NEO {

bool RootDeviceEnvironment::initOsInterface(std::unique_ptr<HwDeviceId> &&hwDeviceId, uint32_t rootDeviceIndex) {
    return initDrmOsInterface(std::move(hwDeviceId), rootDeviceIndex, this, this->osInterface, this->memoryOperationsInterface);
}

} // namespace NEO
