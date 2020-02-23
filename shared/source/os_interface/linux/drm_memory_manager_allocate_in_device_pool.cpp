/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {
GraphicsAllocation *DrmMemoryManager::allocateGraphicsMemoryInDevicePool(const AllocationData &allocationData, AllocationStatus &status) {
    status = AllocationStatus::RetryInNonDevicePool;
    return nullptr;
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(GraphicsAllocation &graphicsAllocation) {
    return nullptr;
}

void *DrmMemoryManager::lockResourceInLocalMemoryImpl(BufferObject *bo) {
    return nullptr;
}

void DrmMemoryManager::unlockResourceInLocalMemoryImpl(BufferObject *bo) {
}

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, const void *memoryToCopy, size_t sizeToCopy) {
    return MemoryManager::copyMemoryToAllocation(graphicsAllocation, memoryToCopy, sizeToCopy);
}

uint64_t DrmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex) {
    return 0 * GB;
}
} // namespace NEO
