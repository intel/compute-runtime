/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

const GfxMemoryAllocationMethod preferredAllocationMethod = GfxMemoryAllocationMethod::useUmdSystemPtr;

size_t WddmMemoryManager::getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod, bool isLocalMemory) const {
    if (NEO::debugManager.flags.ForceWddmHugeChunkSizeMB.get() != -1) {
        return NEO::debugManager.flags.ForceWddmHugeChunkSizeMB.get() * MemoryConstants::megaByte;
    }
    if (isLocalMemory) {
        return 256 * MemoryConstants::megaByte;
    }
    return 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k;
}

} // namespace NEO
