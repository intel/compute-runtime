/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/fixtures/cpu_page_fault_manager_tests_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace NEO {
struct MockSVMAllocsManager : public SVMAllocsManager {
  public:
    using SVMAllocsManager::insertSVMAlloc;
    using SVMAllocsManager::memoryManager;
    using SVMAllocsManager::mtxForIndirectAccess;
    using SVMAllocsManager::svmAllocs;
    using SVMAllocsManager::SVMAllocsManager;
    using SVMAllocsManager::svmDeferFreeAllocs;
    using SVMAllocsManager::svmMapOperations;
    using SVMAllocsManager::usmDeviceAllocationsCache;
    using SVMAllocsManager::usmHostAllocationsCache;

    void prefetchMemory(Device &device, CommandStreamReceiver &commandStreamReceiver, const void *ptr, const size_t size) override {
        SVMAllocsManager::prefetchMemory(device, commandStreamReceiver, ptr, size);
        prefetchMemoryCalled = true;
    }
    bool prefetchMemoryCalled = false;

    void *createUnifiedMemoryAllocation(size_t size, const UnifiedMemoryProperties &memoryProperties) override {
        requestedZeroedOutAllocation = memoryProperties.isInternalAllocation;
        if (createUnifiedMemoryAllocationCallBase) {
            return SVMAllocsManager::createUnifiedMemoryAllocation(size, memoryProperties);
        }
        return createUnifiedMemoryAllocationReturnValue;
    }
    bool requestedZeroedOutAllocation = false;
    bool createUnifiedMemoryAllocationCallBase = true;
    void *createUnifiedMemoryAllocationReturnValue = nullptr;
};

template <bool enableLocalMemory>
struct SVMMemoryAllocatorFixture {
    SVMMemoryAllocatorFixture() : executionEnvironment(defaultHwInfo.get()) {}

    void setUp() {
        executionEnvironment.initGmm();
        memoryManager = std::make_unique<MockMemoryManager>(false, enableLocalMemory, executionEnvironment);
        svmManager = std::make_unique<MockSVMAllocsManager>(memoryManager.get());
        if (enableLocalMemory) {
            memoryManager->pageFaultManager.reset(new MockPageFaultManager);
        }
    }
    void tearDown() {
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<MockMemoryManager> memoryManager;
    std::unique_ptr<MockSVMAllocsManager> svmManager;
    RootDeviceIndicesContainer rootDeviceIndices = {mockRootDeviceIndex};
    std::map<uint32_t, DeviceBitfield> deviceBitfields{{mockRootDeviceIndex, mockDeviceBitfield}};
};

} // namespace NEO
