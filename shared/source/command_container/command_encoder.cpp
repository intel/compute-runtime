/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "shared/source/device/device.h"

namespace NEO {

bool EncodePostSyncArgs::requiresSystemMemoryFence() const {
    return (isHostScopeSignalEvent && isUsingSystemAllocation && this->device->getProductHelper().isGlobalFenceInPostSyncRequired(this->device->getHardwareInfo()));
}

} // namespace NEO
