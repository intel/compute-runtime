/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/mem_obj/mem_obj_helper.h"
#include "runtime/memory_manager/os_agnostic_memory_manager.h"
#include "test.h"
#include "unit_tests/mocks/mock_execution_environment.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryManagerTest, givenSetUseSytemMemoryWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(MemoryManagerTest, givenAllowed32BitAndFroce32BitWhenGraphicsAllocationInDevicePoolIsAllocatedThenNullptrIsReturned) {
    MockExecutionEnvironment executionEnvironment(*platformDevices);
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    memoryManager.setForce32BitAllocations(true);

    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.allow32Bit = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager.allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}

TEST(AllocationFlagsTest, givenAllocateMemoryFlagWhenGetAllocationFlagsIsCalledThenAllocateFlagIsCorrectlySet) {
    auto allocationProperties = MemObjHelper::getAllocationProperties({}, true, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_TRUE(allocationProperties.flags.allocateMemory);

    allocationProperties = MemObjHelper::getAllocationProperties({}, false, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_FALSE(allocationProperties.flags.allocateMemory);
}

TEST(UncacheableFlagsTest, givenUncachedResourceFlagWhenGetAllocationFlagsIsCalledThenUncacheableFlagIsCorrectlySet) {
    MemoryProperties memoryProperties;
    memoryProperties.flags_intel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    auto allocationFlags = MemObjHelper::getAllocationProperties(memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_TRUE(allocationFlags.flags.uncacheable);

    memoryProperties.flags_intel = 0;
    allocationFlags = MemObjHelper::getAllocationProperties(memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_FALSE(allocationFlags.flags.uncacheable);
}

TEST(AllocationFlagsTest, givenReadOnlyResourceFlagWhenGetAllocationFlagsIsCalledThenFlushL3FlagsAreCorrectlySet) {
    auto allocationFlags =
        MemObjHelper::getAllocationProperties(CL_MEM_READ_ONLY, true, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);

    allocationFlags = MemObjHelper::getAllocationProperties(0, true, 0, GraphicsAllocation::AllocationType::BUFFER);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForWrite);
}

TEST(StorageInfoTest, whenStorageInfoIsCreatedWithDefaultConstructorThenReturnsOneHandle) {
    StorageInfo storageInfo;
    EXPECT_EQ(1u, storageInfo.getNumHandles());
}
