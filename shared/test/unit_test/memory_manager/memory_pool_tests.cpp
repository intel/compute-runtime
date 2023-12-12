/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_pool.h"

#include "gtest/gtest.h"

TEST(MemoryPool, givenSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenTrueIsReturned) {
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::system4KBPages));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::system4KBPagesWith32BitGpuAddressing));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::system64KBPages));
    EXPECT_TRUE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::system64KBPagesWith32BitGpuAddressing));
}

TEST(MemoryPool, givenNonSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenFalseIsReturned) {
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::memoryNull));
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::systemCpuInaccessible));
    EXPECT_FALSE(NEO::MemoryPoolHelper::isSystemMemoryPool(NEO::MemoryPool::localMemory));
}
