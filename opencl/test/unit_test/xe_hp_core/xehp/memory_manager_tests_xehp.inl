/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using MemoryManagerTestsXeHP = ::testing::Test;
XEHPTEST_F(MemoryManagerTestsXeHP, givenEnabledLocalMemoryWhenLinearStreamIsAllocatedInPreferredPoolThenLocalMemoryPoolIsUsed) {
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());
    MockMemoryManager memoryManager(false, true, executionEnvironment);

    auto allocation = memoryManager.allocateGraphicsMemoryInPreferredPool({mockRootDeviceIndex, MemoryConstants::pageSize, AllocationType::LINEAR_STREAM, mockDeviceBitfield}, nullptr);
    EXPECT_EQ(MemoryPool::LocalMemory, allocation->getMemoryPool());
    EXPECT_TRUE(memoryManager.allocationInDevicePoolCreated);

    memoryManager.freeGraphicsMemory(allocation);
}
