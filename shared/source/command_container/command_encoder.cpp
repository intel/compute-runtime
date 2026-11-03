/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "shared/source/device/device.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

bool EncodePostSyncArgs::requiresSystemMemoryFence() const {
    return (isHostScopeSignalEvent && isUsingSystemAllocation && this->device->getProductHelper().isGlobalFenceInPostSyncRequired(this->device->getHardwareInfo()));
}

} // namespace NEO
