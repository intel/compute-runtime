/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

TEST(MemoryManagerTest, WhenCallingIsAllocationTypeToCaptureThenScratchAndPrivateTypesReturnTrue) {
    MockMemoryManager mockMemoryManager;

    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::SCRATCH_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::PRIVATE_SURFACE));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::LINEAR_STREAM));
    EXPECT_TRUE(mockMemoryManager.isAllocationTypeToCapture(AllocationType::INTERNAL_HEAP));
}
