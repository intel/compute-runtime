/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
GraphicsAllocation *WddmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    return nullptr;
}
bool WddmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, uint32_t sizeToCopy) const {
    return MemoryManager::copyMemoryToAllocation(graphicsAllocation, memoryToCopy, sizeToCopy);
}
bool WddmMemoryManager::mapGpuVirtualAddress(WddmAllocation *allocation, const void *requiredPtr) {
    return mapGpuVaForOneHandleAllocation(allocation, requiredPtr);
}
} // namespace NEO
