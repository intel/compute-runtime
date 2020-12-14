/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/unified_memory_manager.h"

namespace NEO {

GraphicsAllocation::AllocationType SVMAllocsManager::getGraphicsAllocationType(const UnifiedMemoryProperties &unifiedMemoryProperties) const {
    GraphicsAllocation::AllocationType allocationType = GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY;
    if (unifiedMemoryProperties.memoryType == InternalMemoryType::DEVICE_UNIFIED_MEMORY) {
        if (unifiedMemoryProperties.allocationFlags.allocFlags.allocWriteCombined) {
            allocationType = GraphicsAllocation::AllocationType::WRITE_COMBINED;
        } else {
            allocationType = GraphicsAllocation::AllocationType::BUFFER;
        }
    }
    return allocationType;
}

} // namespace NEO
