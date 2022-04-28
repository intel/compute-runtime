/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/source/memory_manager/os_agnostic_memory_manager.h"

namespace NEO {

constexpr uint32_t mockRootDeviceIndex = 0u;
constexpr DeviceBitfield mockDeviceBitfield(0b1);

class MockGraphicsAllocation : public MemoryAllocation {
  public:
    using MemoryAllocation::allocationOffset;
    using MemoryAllocation::allocationType;
    using MemoryAllocation::aubInfo;
    using MemoryAllocation::gpuAddress;
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::memoryPool;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::size;
    using MemoryAllocation::usageInfos;

    MockGraphicsAllocation()
        : MemoryAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount) {}

    MockGraphicsAllocation(void *buffer, size_t sizeIn)
        : MemoryAllocation(0, AllocationType::UNKNOWN, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount) {}

    MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn)
        : MemoryAllocation(0, AllocationType::UNKNOWN, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount) {}

    MockGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : MemoryAllocation(rootDeviceIndex, AllocationType::UNKNOWN, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount) {}

    void resetInspectionIds() {
        for (auto &usageInfo : usageInfos) {
            usageInfo.inspectionId = 0u;
        }
    }

    void overrideMemoryPool(MemoryPool::Type pool) {
        this->memoryPool = pool;
    }
};

namespace GraphicsAllocationHelper {

static inline MultiGraphicsAllocation toMultiGraphicsAllocation(GraphicsAllocation *graphicsAllocation) {
    MultiGraphicsAllocation multiGraphicsAllocation(graphicsAllocation->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(graphicsAllocation);
    return multiGraphicsAllocation;
}

} // namespace GraphicsAllocationHelper
} // namespace NEO
