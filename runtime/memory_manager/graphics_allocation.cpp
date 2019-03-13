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

GraphicsAllocation::GraphicsAllocation(AllocationType allocationType, void *cpuPtrIn, uint64_t gpuAddress, uint64_t baseAddress,
                                       size_t sizeIn, MemoryPool::Type pool, bool multiOsContextCapable)
    : gpuBaseAddress(baseAddress),
      gpuAddress(gpuAddress),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType) {
    allocationInfo.flags.multiOsContextCapable = multiOsContextCapable;
}

GraphicsAllocation::GraphicsAllocation(AllocationType allocationType, void *cpuPtrIn, size_t sizeIn, osHandle sharedHandleIn,
                                       MemoryPool::Type pool, bool multiOsContextCapable)
    : gpuAddress(castToUint64(cpuPtrIn)),
      size(sizeIn),
      cpuPtr(cpuPtrIn),
      memoryPool(pool),
      allocationType(allocationType) {
    sharingInfo.sharedHandle = sharedHandleIn;
    allocationInfo.flags.multiOsContextCapable = multiOsContextCapable;
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

constexpr uint32_t GraphicsAllocation::objectNotUsed;
constexpr uint32_t GraphicsAllocation::objectNotResident;
} // namespace OCLRT
