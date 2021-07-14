/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

const GfxMemoryAllocationMethod preferredAllocationMethod = GfxMemoryAllocationMethod::AllocateByKmd;

size_t WddmMemoryManager::getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const {
    if (GfxMemoryAllocationMethod::AllocateByKmd == allocationMethod) {
        return 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k;
    } else {
        return 31 * MemoryConstants::megaByte;
    }
}

} // namespace NEO
