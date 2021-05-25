/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/utilities/stackvec.h"

#include <cstdlib>
#include <cstring>
#include <memory>

namespace NEO {

bool canUseAdapterBasedOnDriverDesc(const char *driverDescription) {
    return (strstr(driverDescription, "Intel") != nullptr) ||
           (strstr(driverDescription, "Citrix") != nullptr) ||
           (strstr(driverDescription, "Virtual Render") != nullptr);
}

bool isAllowedDeviceId(uint32_t deviceId) {
    if (DebugManager.flags.ForceDeviceId.get() == "unk") {
        return true;
    }

    char *endptr = nullptr;
    auto reqDeviceId = strtoul(DebugManager.flags.ForceDeviceId.get().c_str(), &endptr, 16);
    return (static_cast<uint32_t>(reqDeviceId) == deviceId);
}

} // namespace NEO
