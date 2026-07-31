/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"

namespace NEO {

bool UsmMemAllocPoolsFacade::isPoolManagerSupported(InternalMemoryType memoryType, const Device *device) {
    bool poolManagerSupported = device->getGfxCoreHelper().isUsmPoolManagerSupported(memoryType);
    if (NEO::debugManager.flags.EnableUsmAllocationPoolManager.get() != -1) {
        poolManagerSupported = NEO::debugManager.flags.EnableUsmAllocationPoolManager.get() != 0;
    }
    return poolManagerSupported;
}

} // namespace NEO
