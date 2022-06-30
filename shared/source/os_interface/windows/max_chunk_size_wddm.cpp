/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

const GfxMemoryAllocationMethod preferredAllocationMethod = GfxMemoryAllocationMethod::UseUmdSystemPtr;

size_t WddmMemoryManager::getHugeGfxMemoryChunkSize(GfxMemoryAllocationMethod allocationMethod) const {
    return 4 * MemoryConstants::gigaByte - MemoryConstants::pageSize64k;
}

} // namespace NEO
