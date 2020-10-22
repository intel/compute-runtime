/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"

namespace NEO {
bool DrmMemoryManager::createDrmAllocation(Drm *drm, DrmAllocation *allocation, uint64_t gpuAddress, size_t maxOsContextCount) {
    return false;
}

BufferObject *DrmMemoryManager::createBufferObjectInMemoryRegion(Drm *drm, uint64_t gpuAddress, size_t size, uint32_t memoryBanks, size_t maxOsContextCount) {
    return nullptr;
}

DrmAllocation *DrmMemoryManager::createAllocWithAlignment(const AllocationData &allocationData, size_t size, size_t alignment, size_t alignedSize, uint64_t gpuAddress) {
    return createAllocWithAlignmentFromUserptr(allocationData, size, alignment, alignedSize, gpuAddress);
}

GraphicsAllocation *DrmMemoryManager::createSharedUnifiedMemoryAllocation(const AllocationData &allocationData) {
    return nullptr;
}

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

bool DrmMemoryManager::copyMemoryToAllocation(GraphicsAllocation *graphicsAllocation, size_t destinationOffset, const void *memoryToCopy, size_t sizeToCopy) {
    return MemoryManager::copyMemoryToAllocation(graphicsAllocation, destinationOffset, memoryToCopy, sizeToCopy);
}

uint64_t DrmMemoryManager::getLocalMemorySize(uint32_t rootDeviceIndex, uint32_t deviceBitfield) {
    return 0 * GB;
}
} // namespace NEO
