/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"

namespace L0 {
namespace Sysman {
namespace ult {

// Helper function to get a valid device ID for survivability mode tests
// Uses deviceDescriptorTable to find any valid device ID for the product being tested
inline uint32_t getValidDeviceIdForProduct() {
    auto deviceId = NEO::defaultHwInfo->platform.usDeviceID;

    // If device ID is already set (non-zero), use it
    if (deviceId != 0) {
        return deviceId;
    }

    // Search deviceDescriptorTable for any device ID matching this product
    auto targetProductFamily = NEO::defaultHwInfo->platform.eProductFamily;
    for (size_t i = 0; NEO::deviceDescriptorTable[i].deviceId != 0; i++) {
        if (NEO::deviceDescriptorTable[i].pHwInfo &&
            NEO::deviceDescriptorTable[i].pHwInfo->platform.eProductFamily == targetProductFamily) {
            return NEO::deviceDescriptorTable[i].deviceId;
        }
    }

    UNRECOVERABLE_IF(true && "No valid device ID found for survivability mode tests. Check deviceDescriptorTable for this platform.");
    return 0;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
