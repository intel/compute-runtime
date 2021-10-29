/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    return nullptr;
}
bool WddmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    return MemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
}
bool WddmMemoryManager::mapGpuVirtualAddress(WddmAllocation *allocation, const void *requiredPtr) {
    if (allocation->getNumGmms() > 1) {
        return mapMultiHandleAllocationWithRetry(allocation, requiredPtr);
    }
    return mapGpuVaForOneHandleAllocation(allocation, requiredPtr);
}

uint64_t WddmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    return 0 * GB;
}
} // namespace NEO
