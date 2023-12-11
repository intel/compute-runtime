/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties) {
    bool preferredCompression = false;
    int32_t compressionEnabled = debugManager.flags.EnableUsmCompression.get();
    if (compressionEnabled == 1) {
        if ((properties.allocationType == AllocationType::svmGpu) ||
            (properties.flags.isUSMDeviceAllocation)) {
            preferredCompression = true;
        }
    }
    return preferredCompression;
}
} // namespace NEO
