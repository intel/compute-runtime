/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

void Device::getAdapterLuid(std::array<uint8_t, ProductHelper::luidSize> &luid) {
    const LUID adapterLuid = getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>()->getAdapterLuid();
    memcpy_s(&luid, ProductHelper::luidSize, &adapterLuid, sizeof(luid));
}

bool Device::verifyAdapterLuid() {
    const LUID adapterLuid = getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>()->getAdapterLuid();
    return getRootDeviceEnvironment().osInterface->getDriverModel()->as<Wddm>()->verifyAdapterLuid(adapterLuid);
}
} // namespace NEO
