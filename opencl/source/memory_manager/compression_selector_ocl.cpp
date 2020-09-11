/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"

namespace NEO {
bool CompressionSelector::preferRenderCompressedBuffer(const AllocationProperties &properties) {
    return GraphicsAllocation::AllocationType::BUFFER_COMPRESSED == properties.allocationType;
}

} // namespace NEO
