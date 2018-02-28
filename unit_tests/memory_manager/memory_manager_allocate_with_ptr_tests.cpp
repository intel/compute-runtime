/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
