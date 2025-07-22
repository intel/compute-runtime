/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    switch (properties.allocationType) {
    case AllocationType::globalSurface:
    case AllocationType::constantSurface:
    case AllocationType::svmGpu:
    case AllocationType::printfSurface: {
        return CompressionSelector::allowStatelessCompression();
    }
    default:
        return false;
    }
}

} // namespace NEO
