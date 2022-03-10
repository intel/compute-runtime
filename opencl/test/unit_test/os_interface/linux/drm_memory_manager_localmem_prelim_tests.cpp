/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/linux/drm_memory_manager_prelim_fixtures.h"

#include "gtest/gtest.h"

TEST_F(DrmMemoryManagerLocalMemoryWithCustomPrelimMockTest, givenDrmMemoryManagerWithLocalMemoryWhenLockResourceIsCalledOnBufferObjectThenReturnPtr) {
    BufferObject bo(mock, 1, 1024, 1);

    DrmAllocation drmAllocation(0, AllocationType::UNKNOWN, &bo, nullptr, 0u, 0u, MemoryPool::LocalMemory);
    EXPECT_EQ(&bo, drmAllocation.getBO());

    auto ptr = memoryManager->lockResourceInLocalMemoryImpl(&bo);
    EXPECT_NE(nullptr, ptr);
    EXPECT_EQ(ptr, bo.peekLockedAddress());

    memoryManager->unlockResourceInLocalMemoryImpl(&bo);
    EXPECT_EQ(nullptr, bo.peekLockedAddress());
}
