/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/memory_manager/memory_manager_allocate_in_device_pool_tests.inl"

TEST(MemoryManagerTest, givenNotSetUseSystemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenAllocationIsReturned) {
    ExecutionEnvironment executionEnvironment;
    OsAgnosticMemoryManager memoryManager(false, false, executionEnvironment);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Error;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_NE(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::Success, status);

    memoryManager.freeGraphicsMemory(allocation);
}
