/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "runtime/helpers/memory_properties_flags_helpers.h"
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
    auto allocationProperties = MemoryPropertiesParser::getAllocationProperties(0, {}, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationProperties.flags.allocateMemory);

    auto allocationProperties2 = MemoryPropertiesParser::getAllocationProperties(0, {}, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationProperties2.flags.allocateMemory);
}

TEST(UncacheableFlagsTest, givenUncachedResourceFlagWhenGetAllocationFlagsIsCalledThenUncacheableFlagIsCorrectlySet) {
    cl_mem_flags_intel flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, flagsIntel, 0);
    auto allocationFlags = MemoryPropertiesParser::getAllocationProperties(0, memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationFlags.flags.uncacheable);

    flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(0, flagsIntel, 0);
    auto allocationFlags2 = MemoryPropertiesParser::getAllocationProperties(0, memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationFlags2.flags.uncacheable);
}

TEST(AllocationFlagsTest, givenReadOnlyResourceFlagWhenGetAllocationFlagsIsCalledThenFlushL3FlagsAreCorrectlySet) {
    cl_mem_flags flags = CL_MEM_READ_ONLY;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(flags, 0, 0);

    auto allocationFlags =
        MemoryPropertiesParser::getAllocationProperties(0, memoryProperties, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);

    auto allocationFlags2 = MemoryPropertiesParser::getAllocationProperties(0, {}, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationFlags2.flags.flushL3RequiredForRead);
    EXPECT_TRUE(allocationFlags2.flags.flushL3RequiredForWrite);
}

TEST(StorageInfoTest, whenStorageInfoIsCreatedWithDefaultConstructorThenReturnsOneHandle) {
    StorageInfo storageInfo;
    EXPECT_EQ(1u, storageInfo.getNumHandles());
}
