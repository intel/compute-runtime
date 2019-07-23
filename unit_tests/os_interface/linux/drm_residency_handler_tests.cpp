/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/linux/drm_residency_handler.h"
#include "test.h"
#include "unit_tests/mocks/mock_graphics_allocation.h"

#include <memory>

using namespace NEO;

struct DrmResidencyHandlerTest : public ::testing::Test {
    void SetUp() override {
        drmResidencyHandler = std::make_unique<DrmResidencyHandler>();
    }

    MockGraphicsAllocation graphicsAllocation;
    std::unique_ptr<DrmResidencyHandler> drmResidencyHandler;
};

TEST_F(DrmResidencyHandlerTest, whenMakingResidentAllocaionExpectMakeResidentFail) {
    EXPECT_FALSE(drmResidencyHandler->makeResident(graphicsAllocation));
    EXPECT_FALSE(drmResidencyHandler->isResident(graphicsAllocation));
}

TEST_F(DrmResidencyHandlerTest, whenEvictingResidentAllocationExpectEvictFalse) {
    EXPECT_FALSE(drmResidencyHandler->makeResident(graphicsAllocation));
    EXPECT_FALSE(drmResidencyHandler->evict(graphicsAllocation));
    EXPECT_FALSE(drmResidencyHandler->isResident(graphicsAllocation));
}
