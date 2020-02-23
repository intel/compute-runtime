/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"

uint32_t UnifiedMemoryControls::generateMask() {
    uint32_t resourceMask = 0u;
    if (this->indirectHostAllocationsAllowed) {
        resourceMask |= InternalMemoryType::HOST_UNIFIED_MEMORY;
    }
    if (this->indirectDeviceAllocationsAllowed) {
        resourceMask |= InternalMemoryType::DEVICE_UNIFIED_MEMORY;
    }
    if (this->indirectSharedAllocationsAllowed) {
        resourceMask |= InternalMemoryType::SHARED_UNIFIED_MEMORY;
    }

    return resourceMask;
}
