/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/buffer_pool_allocator.h"

namespace NEO {
SmallBuffersParams SmallBuffersParams::getDefaultParams() {
    return {
        .aggregatedSmallBuffersPoolSize = MemoryConstants::pageSize64k,
        .smallBufferThreshold = 4 * MemoryConstants::pageSize,
        .chunkAlignment = MemoryConstants::pageSize,
        .startingOffset = MemoryConstants::pageSize};
}

SmallBuffersParams SmallBuffersParams::getLargePagesParams() {
    return {
        .aggregatedSmallBuffersPoolSize = 2 * MemoryConstants::pageSize64k,
        .smallBufferThreshold = 8 * MemoryConstants::pageSize,
        .chunkAlignment = MemoryConstants::pageSize,
        .startingOffset = MemoryConstants::pageSize};
}
} // namespace NEO