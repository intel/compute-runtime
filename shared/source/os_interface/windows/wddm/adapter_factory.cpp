/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/utilities/stackvec.h"

#include <cstring>

namespace NEO {

bool canUseAdapterBasedOnDriverDesc(const char *driverDescription) {
    return (strstr(driverDescription, "Intel") != nullptr) ||
           (strstr(driverDescription, "Citrix") != nullptr) ||
           (strstr(driverDescription, "Virtual Render") != nullptr);
}

bool isAllowedDeviceId(uint32_t deviceId) {
    return DeviceFactory::isAllowedDeviceId(deviceId, debugManager.flags.FilterDeviceId.get());
}

} // namespace NEO
