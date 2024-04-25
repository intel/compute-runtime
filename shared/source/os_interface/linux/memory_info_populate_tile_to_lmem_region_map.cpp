/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/memory_info.h"

namespace NEO {

void MemoryInfo::populateTileToLocalMemoryRegionIndexMap() {
    for (uint32_t i{0u}; i < tileToLocalMemoryRegionIndexMap.size(); i++) {
        tileToLocalMemoryRegionIndexMap[i] = i;
    }
}
} // namespace NEO