/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_default.h"

#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include <memory>

using namespace NEO;

struct MockDrmMemoryOperationsHandlerDefault : public DrmMemoryOperationsHandlerDefault {
    using DrmMemoryOperationsHandlerDefault::residency;
};

struct DrmMemoryOperationsHandlerBaseTest : public ::testing::Test {
    void SetUp() override {
        drmMemoryOperationsHandler = std::make_unique<MockDrmMemoryOperationsHandlerDefault>();

        allocationPtr = &graphicsAllocation;
    }

    MockGraphicsAllocation graphicsAllocation;
    GraphicsAllocation *allocationPtr;
    std::unique_ptr<MockDrmMemoryOperationsHandlerDefault> drmMemoryOperationsHandler;
};

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenMakingResidentAllocaionExpectMakeResidentFail) {
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(allocationPtr) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, graphicsAllocation), MemoryOperationsStatus::SUCCESS);
}

TEST_F(DrmMemoryOperationsHandlerBaseTest, whenEvictingResidentAllocationExpectEvictFalse) {
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
    EXPECT_EQ(drmMemoryOperationsHandler->makeResident(nullptr, ArrayRef<GraphicsAllocation *>(&allocationPtr, 1)), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, graphicsAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 1u);
    EXPECT_TRUE(drmMemoryOperationsHandler->residency.find(allocationPtr) != drmMemoryOperationsHandler->residency.end());
    EXPECT_EQ(drmMemoryOperationsHandler->evict(nullptr, graphicsAllocation), MemoryOperationsStatus::SUCCESS);
    EXPECT_EQ(drmMemoryOperationsHandler->isResident(nullptr, graphicsAllocation), MemoryOperationsStatus::MEMORY_NOT_FOUND);
    EXPECT_EQ(drmMemoryOperationsHandler->residency.size(), 0u);
}
