/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_pool.h"

#include "gtest/gtest.h"

TEST(MemoryPool, givenSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::System4KBPages));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::System4KBPagesWith32BitGpuAddressing));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::System64KBPages));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::System64KBPagesWith32BitGpuAddressing));
}

TEST(MemoryPool, givenNonSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::MemoryNull));
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::SystemCpuInaccessible));
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::LocalMemory));
}
