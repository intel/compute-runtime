/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_memory_manager.h"
#include "unit_tests/os_interface/windows/wddm_memory_manager_tests.h"
#include "gtest/gtest.h"
using namespace OCLRT;
using namespace ::testing;

TEST_F(WddmMemoryManagerSimpleTest, givenUseSystemMemorySetToTrueWhenAllocateInDevicePoolIsCalledThenNullptrIsReturned) {
    memoryManager.reset(new MockWddmMemoryManager(false, false, wddm));
    MemoryManager::AllocationStatus status = MemoryManager::AllocationStatus::Success;
    AllocationData allocData;
    allocData.allFlags = 0;
    allocData.size = MemoryConstants::pageSize;
    allocData.flags.useSystemMemory = true;
    allocData.flags.allocateMemory = true;

    auto allocation = memoryManager->allocateGraphicsMemoryInDevicePool(allocData, status);
    EXPECT_EQ(nullptr, allocation);
    EXPECT_EQ(MemoryManager::AllocationStatus::RetryInNonDevicePool, status);
}
