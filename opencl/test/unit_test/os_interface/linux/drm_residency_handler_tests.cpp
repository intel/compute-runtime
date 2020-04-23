/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include <memory>

using namespace NEO;

struct DrmMemoryOperationsHandlerTest : public ::testing::Test {
    void SetUp() override {
        drmMemoryOperationsHandler = std::make_unique<DrmMemoryOperationsHandler>();

        allocationPtr = &graphicsAllocation;
    }

    MockGraphicsAllocation graphicsAllocation;
    GraphicsAllocation *allocationPtr;
    std::unique_ptr<DrmMemoryOperationsHandler> drmMemoryOperationsHandler;
};

TEST_F(DrmMemoryOperationsHandlerTest, whenMakingResidentAllocaionExpectMakeResidentFail) {
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->getResidencySet().size(), 1u);
    EXPECT_EQ(*drmMemoryOperationsHandler->getResidencySet().find(allocationPtr), allocationPtr);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(graphicsAllocation), MemoryOperationsStatus::SUCCESS);
}

TEST_F(DrmMemoryOperationsHandlerTest, whenEvictingResidentAllocationExpectEvictFalse) {
    EXPECT_EQ(drmMemoryOperationsHandler->getResidencySet().size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(graphicsAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->getResidencySet().size(), 1u);
    EXPECT_EQ(*drmMemoryOperationsHandler->getResidencySet().find(allocationPtr), allocationPtr);
    EXPECT_EQ(drmMemoryOperationsHandler->evict(graphicsAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(graphicsAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(drmMemoryOperationsHandler->getResidencySet().size(), 0u);
}
