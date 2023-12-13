/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/unified_memory/unified_memory.h"

uint32_t UnifiedMemoryControls::generateMask() {
    uint32_t resourceMask = 0u;
    if (this->indirectHostAllocationsAllowed) {
        resourceMask |= static_cast<uint32_t>(InternalMemoryType::hostUnifiedMemory);
    }
    if (this->indirectDeviceAllocationsAllowed) {
        resourceMask |= static_cast<uint32_t>(InternalMemoryType::deviceUnifiedMemory);
    }
    if (this->indirectSharedAllocationsAllowed) {
        resourceMask |= static_cast<uint32_t>(InternalMemoryType::sharedUnifiedMemory);
    }

    return resourceMask;
}
