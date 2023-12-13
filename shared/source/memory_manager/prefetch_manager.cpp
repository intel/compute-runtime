/*
 * Copyright (C) 2022-2023 Intel Corporation
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

void PrefetchManager::insertAllocation(PrefetchContext &context, const void *usmPtr, SvmAllocationData &allocData) {
    std::unique_lock<SpinLock> lock{context.lock};
    if (allocData.memoryType == InternalMemoryType::sharedUnifiedMemory) {
        context.allocations.push_back(usmPtr);
    }
}

void PrefetchManager::migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr) {
    std::unique_lock<SpinLock> lock{context.lock};
    for (auto &ptr : context.allocations) {
        auto allocData = unifiedMemoryManager.getSVMAlloc(ptr);
        if (allocData) {
            unifiedMemoryManager.prefetchMemory(device, csr, *allocData);
        }
    }
}

void PrefetchManager::removeAllocations(PrefetchContext &context) {
    std::unique_lock<SpinLock> lock{context.lock};
    context.allocations.clear();
}

} // namespace NEO
