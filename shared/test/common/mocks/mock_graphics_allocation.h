/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

inline constexpr uint32_t mockRootDeviceIndex = 0u;
inline constexpr DeviceBitfield mockDeviceBitfield(0b1);

class MockGraphicsAllocation : public MemoryAllocation {
  public:
    using MemoryAllocation::allocationOffset;
    using MemoryAllocation::allocationType;
    using MemoryAllocation::aubInfo;
    using MemoryAllocation::cpuPtr;
    using MemoryAllocation::gpuAddress;
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::memoryPool;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::size;
    using MemoryAllocation::usageInfos;

    MockGraphicsAllocation()
        : MemoryAllocation(0, AllocationType::UNKNOWN, nullptr, 0u, 0, MemoryPool::MemoryNull, MemoryManager::maxOsContextCount, 0llu) {}

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

    void overrideMemoryPool(MemoryPool pool) {
        this->memoryPool = pool;
    }
};

class MockGraphicsAllocationTaskCount : public MockGraphicsAllocation {
  public:
    TaskCountType getTaskCount(uint32_t contextId) const override {
        getTaskCountCalleedTimes++;
        return MockGraphicsAllocation::getTaskCount(contextId);
    }
    void updateTaskCount(TaskCountType newTaskCount, uint32_t contextId) override {
        updateTaskCountCalleedTimes++;
        MockGraphicsAllocation::updateTaskCount(newTaskCount, contextId);
    }
    static uint32_t getTaskCountCalleedTimes;
    uint32_t updateTaskCountCalleedTimes = 0;
};

namespace GraphicsAllocationHelper {

static inline MultiGraphicsAllocation toMultiGraphicsAllocation(GraphicsAllocation *graphicsAllocation) {
    MultiGraphicsAllocation multiGraphicsAllocation(graphicsAllocation->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(graphicsAllocation);
    return multiGraphicsAllocation;
}

} // namespace GraphicsAllocationHelper
} // namespace NEO
