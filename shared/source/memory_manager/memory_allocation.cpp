/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_allocation.h"

#include "shared/source/debug_settings/debug_settings_manager.h"

namespace NEO {

void MemoryAllocation::overrideMemoryPool(MemoryPool pool) {
    if (DebugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
        this->memoryPool = MemoryPool::LocalMemory;
        return;
    }
    this->memoryPool = pool;
}

} // namespace NEO