/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

const GfxMemoryAllocationMethod preferredAllocationMethod = GfxMemoryAllocationMethod::useUmdSystemPtr;

size_t WddmMemoryManager::getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const {
    if (NEO::debugManager.flags.ForceWddmHugeChunkSizeMB.get() != -1) {
        return NEO::debugManager.flags.ForceWddmHugeChunkSizeMB.get() * MemoryConstants::megaByte;
    }
    return 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k;
}

} // namespace NEO
