/*
 * Copyright (C) 2018-2020 Intel Corporation
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
bool WddmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy) {
    return MemoryManager::copyMemoryToAllocation(graphicsAllocation, memoryToCopy, sizeToCopy);
}
bool WddmMemoryManager::mapGpuVirtualAddress(WddmAllocation *allocation, const void *requiredPtr) {
    return mapGpuVaForOneHandleAllocation(allocation, requiredPtr);
}
uint64_t WddmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex) {
    return 0 * GB;
}
} // namespace NEO
