/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    bool preferredCompression = false;
    int32_t compressionEnabled = debugManager.flags.EnableUsmCompression.get();
    if (compressionEnabled == 1) {
        if ((properties.allocationType == AllocationType::svmGpu) ||
            (properties.flags.isHostInaccessibleAllocation)) {
            preferredCompression = true;
        }
    }
    return preferredCompression;
}
} // namespace NEO
