/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/memory_manager/memory_manager_allocate_in_device_pool_tests.inl"

#include "shared/source/helpers/array_count.h"

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenAllocationIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenImageOrSharedResourceCopyWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    GraphicsAllocation::AllocationType types[] = {GraphicsAllocation::AllocationType::IMAGE,
                                                  GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY};

    for (auto type : types) {
        allocData.type = type;
        auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
    }
}

TEST(MemoryManagerTest, givenSvmGpuAllocationTypeWhenAllocationSystemMemoryFailsThenReturnNull) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = GraphicsAllocation::AllocationType::SVM_GPU;
    allocData.hostPtr = reinterpret_cast<void *>(0x1000);

    memoryManager.failAllocateSystemMemory = true;
    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Error, status);
}

TEST(MemoryManagerTest, givenSvmGpuAllocationTypeWhenAllocationSucceedThenReturnGpuAddressAsHostPtr) {
    if (platformDevices[0]->capabilityTable.gpuAddressSpace != maxNBitValue(48) && platformDevices[0]->capabilityTable.gpuAddressSpace != maxNBitValue(47)) {
        return;
    }

    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.type = GraphicsAllocation::AllocationType::SVM_GPU;
    allocData.hostPtr = reinterpret_cast<void *>(0x1000);

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    ASSERT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);
    EXPECT_EQ(reinterpret_cast<uint64_t>(allocData.hostPtr), allocation->getGpuAddress());
    EXPECT_NE(reinterpret_cast<uint64_t>(allocation->getUnderlyingBuffer()), allocation->getGpuAddress());

    memoryManager.freeGraphicsMemory(allocation);
}

TEST(MemoryManagerTest, givenOsAgnosticMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    EXPECT_EQ(0 * GB, memoryManager.getLocalMemorySize(0u));
}
