/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {
GraphicsAllocation *OsAgnosticMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::Error;
    switch (allocationData.type) {
    case GraphicsAllocation::AllocationType::IMAGE:
    case GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY:
        break;
    default:
        if (!allocationData.flags.useSystemMemory && !(allocationData.flags.allow32Bit && this->force32bitAllocations)) {
            GraphicsAllocation *allocation = nullptr;
            if (allocationData.type == GraphicsAllocation::AllocationType::SVM_GPU) {
                void *cpuAllocation = allocateSystemMemory(allocationData.size, allocationData.alignment);
                if (!cpuAllocation) {
                    return nullptr;
                }
                uint64_t gpuAddress = reinterpret_cast<uint64_t>(allocationData.hostPtr);
                allocation = new MemoryAllocation(allocationData.type, cpuAllocation, cpuAllocation, gpuAddress, allocationData.size, counter++, MemoryPool::LocalMemory, false, false, false);
                allocation->setGpuBaseAddress(gpuAddress);
            } else {
                allocation = allocateGraphicsMemory(allocationData);
            }

            if (allocation) {
                allocation->storageInfo = allocationData.storageInfo;
                allocation->setFlushL3Required(allocationData.flags.flushL3);
                status = AllocationStatus::Success;
            }
            return allocation;
        }
    }
    status = AllocationStatus::RetryInNonDevicePool;
    return nullptr;
}

void MemoryAllocation::overrideMemoryPool(MemoryPool::Type pool) {
    this->memoryPool = pool;
}
} // namespace NEO
