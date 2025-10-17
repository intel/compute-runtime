/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/buffer_pool_params.h"

namespace NEO {
SmallBuffersParams SmallBuffersParams::getDefaultParams() {
    return {
        .aggregatedSmallBuffersPoolSize = 16 * MemoryConstants::megaByte,
        .smallBufferThreshold = 2 * MemoryConstants::megaByte,
        .chunkAlignment = MemoryConstants::pageSize64k,
        .startingOffset = MemoryConstants::pageSize64k};
}
} // namespace NEO
