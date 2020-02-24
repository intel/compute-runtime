/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/os_interface/windows/wddm_memory_manager_tests.h"

#include "gtest/gtest.h"
using namespace NEO;
using namespace ::testing;

TEST_F(WddmMemoryManagerSimpleTest, givenUseSystemMemorySetToTrueWhenAllocateInDevicePoolIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, *executionEnvironment));
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}
