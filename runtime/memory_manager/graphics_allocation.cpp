/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/graphics_allocation.h"

#include "runtime/helpers/aligned_memory.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {

void GraphicsAllocation::setAllocationType(AllocationType allocationType) {
    DebugManager.logAllocation(this);
    this->allocationType = allocationType;
}

bool GraphicsAllocation::isL3Capable() {
    auto ptr = ptrOffset(cpuPtr, static_cast<size_t>(this->allocationOffset));
    if (alignUp(ptr, MemoryConstants::cacheLineSize) == ptr && alignUp(this->size, MemoryConstants::cacheLineSize) == this->size) {
        return true;
    }
    return false;
}

GraphicsAllocation::GraphicsAllocation(AllocationType allocationType, void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress,
                                       size_t sizeIn, MemoryPool::Type pool, bool multiOsContextCapable)
    : gpuBaseAddress(baseAddress),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      gpuAddress(gpuAddress),
      memoryPool(pool),
      allocationType(allocationType),
      multiOsContextCapable(multiOsContextCapable) {}

GraphicsAllocation::GraphicsAllocation(AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn,
                                       MemoryPool::Type pool, bool multiOsContextCapable)
    : size(sizeIn),
      cpuPtr(cpuPtrIn),
      gpuAddress(castToUint64(cpuPtrIn)),
      sharedHandle(sharedHandleIn),
      memoryPool(pool),
      allocationType(allocationType),
      multiOsContextCapable(multiOsContextCapable) {}

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

constexpr uint32_t GraphicsAllocation::objectNotUsed;
constexpr uint32_t GraphicsAllocation::objectNotResident;
} // namespace OCLRT
