/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
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
    auto allocationProperties = MemoryPropertiesParser::getAllocationProperties({}, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationProperties.flags.allocateMemory);

    allocationProperties = MemoryPropertiesParser::getAllocationProperties({}, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationProperties.flags.allocateMemory);
}

TEST(UncacheableFlagsTest, givenUncachedResourceFlagWhenGetAllocationFlagsIsCalledThenUncacheableFlagIsCorrectlySet) {
    MemoryProperties properties;
    properties.flagsIntel = CL_MEM_LOCALLY_UNCACHED_RESOURCE;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    auto allocationFlags = MemoryPropertiesParser::getAllocationProperties(memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationFlags.flags.uncacheable);

    properties.flagsIntel = 0;
    memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);
    allocationFlags = MemoryPropertiesParser::getAllocationProperties(memoryProperties, false, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationFlags.flags.uncacheable);
}

TEST(AllocationFlagsTest, givenReadOnlyResourceFlagWhenGetAllocationFlagsIsCalledThenFlushL3FlagsAreCorrectlySet) {
    MemoryProperties properties;
    properties.flags = CL_MEM_READ_ONLY;
    MemoryPropertiesFlags memoryProperties = MemoryPropertiesFlagsParser::createMemoryPropertiesFlags(properties);

    auto allocationFlags =
        MemoryPropertiesParser::getAllocationProperties(memoryProperties, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_FALSE(allocationFlags.flags.flushL3RequiredForWrite);

    allocationFlags = MemoryPropertiesParser::getAllocationProperties({}, true, 0, GraphicsAllocation::AllocationType::BUFFER, false);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForRead);
    EXPECT_TRUE(allocationFlags.flags.flushL3RequiredForWrite);
}

TEST(StorageInfoTest, whenStorageInfoIsCreatedWithDefaultConstructorThenReturnsOneHandle) {
    StorageInfo storageInfo;
    EXPECT_EQ(1u, storageInfo.getNumHandles());
}
