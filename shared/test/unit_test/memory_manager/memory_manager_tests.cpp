/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryManagerTest, WhenCallingIsAllocationTypeToCaptureThenScratchAndPrivateTypesReturnTrue) {
    MockMemoryManager mockMemoryManager;

    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::SCRATCH_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::PRIVATE_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::LINEAR_STREAM));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::INTERNAL_HEAP));
}

TEST(MemoryManagerTest, givenAllocationWithNullCpuPtrThenMemoryCopyToAllocationReturnsFalse) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    constexpr uint8_t allocationSize = 10;
    uint8_t allocationStorage[allocationSize] = {0};
    MockGraphicsAllocation allocation{allocationStorage, allocationSize};
    allocation.cpuPtr = nullptr;
    constexpr size_t offset = 0;

    EXPECT_FALSE(memoryManager.copyMemoryToAllocation(&allocation, offset, nullptr, 0));
}

TEST(MemoryManagerTest, givenDefaultMemoryManagerWhenItIsAskedForBudgetExhaustionThenFalseIsReturned) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, false, executionEnvironment);
    EXPECT_FALSE(memoryManager.isMemoryBudgetExhausted());
}
