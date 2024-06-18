/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_allocation.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/memory_manager/multi_graphics_allocation.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {

inline constexpr uint32_t mockRootDeviceIndex = 0u;
inline constexpr DeviceBitfield mockDeviceBitfield(0b1);

class MockGraphicsAllocation : public MemoryAllocation {
  public:
    using BaseClass = MemoryAllocation;
    using MemoryAllocation::allocationOffset;
    using MemoryAllocation::allocationType;
    using MemoryAllocation::aubInfo;
    using MemoryAllocation::cpuPtr;
    using MemoryAllocation::gpuAddress;
    using MemoryAllocation::MemoryAllocation;
    using MemoryAllocation::memoryPool;
    using MemoryAllocation::objectNotResident;
    using MemoryAllocation::objectNotUsed;
    using MemoryAllocation::residency;
    using MemoryAllocation::size;
    using MemoryAllocation::usageInfos;

    MockGraphicsAllocation()
        : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, nullptr, 0u, 0, MemoryPool::memoryNull, MemoryManager::maxOsContextCount, 0llu) {}

    MockGraphicsAllocation(void *buffer, size_t sizeIn)
        : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    MockGraphicsAllocation(void *buffer, uint64_t gpuAddr, size_t sizeIn)
        : MemoryAllocation(0, 1u /*num gmms*/, AllocationType::unknown, buffer, gpuAddr, 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    MockGraphicsAllocation(uint32_t rootDeviceIndex, void *buffer, size_t sizeIn)
        : MemoryAllocation(rootDeviceIndex, 1u /*num gmms*/, AllocationType::unknown, buffer, castToUint64(buffer), 0llu, sizeIn, MemoryPool::memoryNull, MemoryManager::maxOsContextCount) {}

    void resetInspectionIds() {
        for (auto &usageInfo : usageInfos) {
            usageInfo.inspectionId = 0u;
        }
    }

    void overrideMemoryPool(MemoryPool pool) {
        this->memoryPool = pool;
    }

    int peekInternalHandle(MemoryManager *memoryManager, uint64_t &handle) override {
        handle = internalHandle;
        return peekInternalHandleResult;
    }
    void updateCompletionDataForAllocationAndFragments(uint64_t newFenceValue, uint32_t contextId) override {
        updateCompletionDataForAllocationAndFragmentsCalledtimes++;
        MemoryAllocation::updateCompletionDataForAllocationAndFragments(newFenceValue, contextId);
    }

    ADDMETHOD(hasAllocationReadOnlyType, bool, false, false, (), ());
    ADDMETHOD_VOIDRETURN(setAsReadOnly, false, (), ());
    uint64_t updateCompletionDataForAllocationAndFragmentsCalledtimes = 0;
    int peekInternalHandleResult = 0;
    uint64_t internalHandle = 0;
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
