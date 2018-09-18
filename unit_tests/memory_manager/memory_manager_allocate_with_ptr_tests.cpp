/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gtest/gtest.h"
#include "test.h"
#include "unit_tests/gen_common/matchers.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include <memory>

using namespace OCLRT;
using namespace std;
using namespace ::testing;

TEST(MemoryManagerTest, givenInvalidHostPointerWhenPopulateOsHandlesFailsThenNullAllocationIsReturned) {
    unique_ptr<NiceMock<GMockMemoryManager>> gmockMemoryManager(new NiceMock<GMockMemoryManager>);

    EXPECT_CALL(*gmockMemoryManager, populateOsHandles(::testing::_)).Times(1).WillOnce(::testing::Return(MemoryManager::AllocationStatus::InvalidHostPointer));

    const char memory[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    size_t size = sizeof(memory);

    auto allocation = gmockMemoryManager->allocateGraphicsMemory(size, memory);

    ASSERT_EQ(nullptr, allocation);
    gmockMemoryManager->freeGraphicsMemory(allocation);
}
