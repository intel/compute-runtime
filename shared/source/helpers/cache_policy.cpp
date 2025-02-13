/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/cache_policy.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

bool isL3Capable(const void *ptr, size_t size) {
    return isAligned<MemoryConstants::cacheLineSize>(ptr) &&
           isAligned<MemoryConstants::cacheLineSize>(size);
}

bool isL3Capable(const NEO::GraphicsAllocation &graphicsAllocation) {
    return isL3Capable(graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
}

void L1CachePolicy::init(const ProductHelper &helper) {
    defaultDebuggerActive = helper.getL1CachePolicy(true);
    defaultDebuggerInactive = helper.getL1CachePolicy(false);
}

} // namespace NEO
