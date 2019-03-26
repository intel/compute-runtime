/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/cache_policy.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/memory_manager/graphics_allocation.h"

namespace NEO {

bool isL3Capable(void *ptr, size_t size) {
    return isAligned<MemoryConstants::cacheLineSize>(ptr) &&
           isAligned<MemoryConstants::cacheLineSize>(size);
}

bool isL3Capable(const NEO::GraphicsAllocation &graphicsAllocation) {
    auto ptr = ptrOffset(graphicsAllocation.getUnderlyingBuffer(), static_cast<size_t>(graphicsAllocation.getAllocationOffset()));
    return isL3Capable(ptr, graphicsAllocation.getUnderlyingBufferSize());
}

} // namespace NEO
