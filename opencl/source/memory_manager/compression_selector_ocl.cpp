/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    return false;
}

} // namespace NEO
