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
        .aggregatedSmallBuffersPoolSize = 2 * MemoryConstants::megaByte,
        .smallBufferThreshold = 1 * MemoryConstants::megaByte,
        .chunkAlignment = MemoryConstants::pageSize64k,
        .startingOffset = MemoryConstants::pageSize64k};
}

SmallBuffersParams SmallBuffersParams::getLargePagesParams() {
    return {
        .aggregatedSmallBuffersPoolSize = 16 * MemoryConstants::megaByte,
        .smallBufferThreshold = 2 * MemoryConstants::megaByte,
        .chunkAlignment = MemoryConstants::pageSize64k,
        .startingOffset = MemoryConstants::pageSize64k};
}
} // namespace NEO