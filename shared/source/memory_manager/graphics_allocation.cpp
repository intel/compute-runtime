/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "graphics_allocation.h"

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/utilities/logger.h"

namespace NEO {
void GraphicsAllocation::setAllocationType(AllocationType allocationType) {
    this->allocationType = allocationType;
    FileLoggerInstance().logAllocation(this);
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress,
                                       size_t sizeIn, MemoryPool::Type pool)
    : rootDeviceIndex(rootDeviceIndex),
      gpuBaseAddress(baseAddress),
      gpuAddress(gpuAddress),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(MemoryManager::maxOsContextCount) {
}

GraphicsAllocation::GraphicsAllocation(uint32_t rootDeviceIndex, AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn,
                                       MemoryPool::Type pool)
    : rootDeviceIndex(rootDeviceIndex),
      gpuAddress(castToUint64(cpuPtrIn)),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType),
      usageInfos(MemoryManager::maxOsContextCount) {
    sharingInfo.sharedHandle = sharedHandleIn;
}

GraphicsAllocation::~GraphicsAllocation() = default;

void GraphicsAllocation::updateTaskCount(uint32_t newTaskCount, uint32_t contextId) {
    if (usageInfos[contextId].taskCount == objectNotUsed) {
        registeredContextsNum++;
    }
    if (newTaskCount == objectNotUsed) {
        registeredContextsNum--;
    }
    usageInfos[contextId].taskCount = newTaskCount;
}

std::string GraphicsAllocation::getAllocationInfoString() const {
    return "";
}

uint32_t GraphicsAllocation::getUsedPageSize() const {
    switch (this->memoryPool) {
    case MemoryPool::System64KBPages:
    case MemoryPool::System64KBPagesWith32BitGpuAddressing:
    case MemoryPool::LocalMemory:
        return MemoryConstants::pageSize64k;
    default:
        return MemoryConstants::pageSize;
    }
}

constexpr uint32_t GraphicsAllocation::objectNotUsed;
constexpr uint32_t GraphicsAllocation::objectNotResident;
} // namespace NEO
