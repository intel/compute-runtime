/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/cache_policy.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace OCLRT {

bool isL3Capable(void *ptr, size_t size) {
    if (alignUp(ptr, MemoryConstants::cacheLineSize) == ptr &&
        alignUp(size, MemoryConstants::cacheLineSize) == size) {
        return true;
    }
    return false;
}

bool isL3Capable(const OCLRT::GraphicsAllocation &graphicsAllocation) {
    auto ptr = ptrOffset(graphicsAllocation.getUnderlyingBuffer(), static_cast<size_t>(graphicsAllocation.allocationOffset));
    return isL3Capable(ptr, graphicsAllocation.getUnderlyingBufferSize());
}

} // namespace OCLRT
