/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_memory_operations_handler.h"
#include "test.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include <memory>

using namespace NEO;

struct DrmMemoryOperationsHandlerTest : public ::testing::Test {
    void SetUp() override {
        drmMemoryOperationsHandler = std::make_unique<DrmMemoryOperationsHandler>();
    }

    MockGraphicsAllocation graphicsAllocation;
    std::unique_ptr<DrmMemoryOperationsHandler> drmMemoryOperationsHandler;
};

TEST_F(DrmMemoryOperationsHandlerTest, whenMakingResidentAllocaionExpectMakeResidentFail) {
    EXPECT_FALSE(drmMemoryOperationsHandler->makeResident(graphicsAllocation));
    EXPECT_FALSE(drmMemoryOperationsHandler->isResident(graphicsAllocation));
}

TEST_F(DrmMemoryOperationsHandlerTest, whenEvictingResidentAllocationExpectEvictFalse) {
    EXPECT_FALSE(drmMemoryOperationsHandler->makeResident(graphicsAllocation));
    EXPECT_FALSE(drmMemoryOperationsHandler->evict(graphicsAllocation));
    EXPECT_FALSE(drmMemoryOperationsHandler->isResident(graphicsAllocation));
}
