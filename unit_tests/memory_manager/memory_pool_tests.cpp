/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/array_count.h"
#include "runtime/memory_manager/memory_pool.h"

#include "gtest/gtest.h"

TEST(MemoryPool, givenSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenTrueIsReturned) {

    MemoryPool::Type systemMemoryTypes[] = {MemoryPool::System4KBPages,
                                            MemoryPool::System4KBPagesWith32BitGpuAddressing,
                                            MemoryPool::System64KBPages,
                                            MemoryPool::System64KBPagesWith32BitGpuAddressing};

    for (size_t i = 0; i < arrayCount(systemMemoryTypes); i++) {
        EXPECT_TRUE(MemoryPool::isSystemMemoryPool(systemMemoryTypes[i]));
    }
}

TEST(MemoryPool, givenNonSystemMemoryPoolTypesWhenIsSystemMemoryPoolIsCalledThenFalseIsReturned) {

    MemoryPool::Type memoryTypes[] = {MemoryPool::MemoryNull,
                                      MemoryPool::SystemCpuInaccessible,
                                      MemoryPool::LocalMemory};

    for (size_t i = 0; i < arrayCount(memoryTypes); i++) {
        EXPECT_FALSE(MemoryPool::isSystemMemoryPool(memoryTypes[i]));
    }
}
