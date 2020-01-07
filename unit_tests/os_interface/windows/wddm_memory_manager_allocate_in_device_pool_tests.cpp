/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/os_interface/windows/wddm_memory_manager_allocate_in_device_pool_tests.inl"

TEST_F(WddmMemoryManagerSimpleTest, givenWddmMemoryManagerWhenGetLocalMemoryIsCalledThenSizeOfLocalMemoryIsReturned) {
    EXPECT_EQ(0 * GB, memoryManager->getLocalMemorySize(0u));
}
