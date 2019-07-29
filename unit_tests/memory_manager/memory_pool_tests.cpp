/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_pool.h"

#include "gtest/gtest.h"

TEST(MemoryPool, givenSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(MemoryPool::System4KBPages));
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(MemoryPool::System4KBPagesWith32BitGpuAddressing));
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(MemoryPool::System64KBPages));
    EXPECT_TRUE(MemoryPool::isSystemMemoryPool(MemoryPool::System64KBPagesWith32BitGpuAddressing));
}

TEST(MemoryPool, givenNonSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(MemoryPool::isSystemMemoryPool(MemoryPool::MemoryNull));
    EXPECT_FALSE(MemoryPool::isSystemMemoryPool(MemoryPool::SystemCpuInaccessible));
    EXPECT_FALSE(MemoryPool::isSystemMemoryPool(MemoryPool::LocalMemory));
}
