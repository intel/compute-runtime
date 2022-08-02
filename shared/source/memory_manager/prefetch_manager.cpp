/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"

#include "shared/source/device/device.h"
#include "shared/source/memory_manager/unified_memory_manager.h"

namespace NEO {

std::unique_ptr<PrefetchManager> PrefetchManager::create() {
    return std::make_unique<PrefetchManager>();
}

void PrefetchManager::insertAllocation(SvmAllocationData &svmData) {
    std::unique_lock<SpinLock> lock{mtx};
    if (svmData.memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
        allocations.push_back(svmData);
    }
}

void PrefetchManager::migrateAllocationsToGpu(SVMAllocsManager &unifiedMemoryManager, Device &device) {
    std::unique_lock<SpinLock> lock{mtx};
    for (auto allocData : allocations) {
        unifiedMemoryManager.prefetchMemory(device, allocData);
    }
    allocations.clear();
}

} // namespace NEO
