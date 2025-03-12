/*
 * Copyright (C) 2022-2025 Intel Corporation
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

void PrefetchManager::insertAllocation(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, const void *usmPtr, const size_t size) {
    std::unique_lock<SpinLock> lock{context.lock};
    auto allocData = unifiedMemoryManager.getSVMAlloc(usmPtr);
    if (!allocData) {
        if (device.areSharedSystemAllocationsAllowed()) {
            context.allocations.push_back({usmPtr, size});
        }
    } else if (allocData->memoryType == InternalMemoryType::sharedUnifiedMemory) {
        context.allocations.push_back({usmPtr, size});
    }
}

void PrefetchManager::migrateAllocationsToGpu(PrefetchContext &context, SVMAllocsManager &unifiedMemoryManager, Device &device, CommandStreamReceiver &csr) {
    std::unique_lock<SpinLock> lock{context.lock};
    for (auto &allocation : context.allocations) {
        unifiedMemoryManager.prefetchMemory(device, csr, allocation.ptr, allocation.size);
    }
}

void PrefetchManager::removeAllocations(PrefetchContext &context) {
    std::unique_lock<SpinLock> lock{context.lock};
    context.allocations.clear();
}

} // namespace NEO
