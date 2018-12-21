/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/array_count.h"
#include "unit_tests/memory_manager/memory_manager_allocate_in_device_pool_tests.inl"

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
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
    ExecutionEnvironment executionEnvironment;
    MockMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    GraphicsAllocation::AllocationType types[] = {GraphicsAllocation::AllocationType::IMAGE,
                                                  GraphicsAllocation::AllocationType::SHARED_RESOURCE_COPY};

    for (uint32_t i = 0; i < arrayCount(types); i++) {
        allocData.type = types[i];
        auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
        EXPECT_EQ(nullptr, allocation);
        EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
    }
}
