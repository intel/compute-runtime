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

void PrefetchManager::insertAllocation(PrefetchContext &context, SvmAllocationData &svmData) {
    std::unique_lock<SpinLock> lock{context.lock};
    if (svmData.memoryType == InternalMemoryType::SHARED_UNIFIED_MEMORY) {
        context.allocations.push_back(svmData);
    }
}

void PrefetchManager::migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device) {
    std::unique_lock<SpinLock> lock{context.lock};
    for (auto allocData : context.allocations) {
        unifiedMemoryManager.prefetchMemory(device, allocData);
    }
}

void PrefetchManager::removeAllocations(PrefetchContext &context) {
    std::unique_lock<SpinLock> lock{context.lock};
    context.allocations.clear();
}

} // namespace NEO
