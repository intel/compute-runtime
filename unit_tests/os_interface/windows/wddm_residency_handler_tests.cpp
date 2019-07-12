/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_residency_handler.h"
#include "test.h"
#include "unit_tests/mocks/mock_allocation_properties.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_wddm_allocation.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

using namespace NEO;

struct WddmResidencyHandlerTest : public WddmTest {
    void SetUp() override {
        WddmTest::SetUp();
        wddmResidencyHandler = std::make_unique<WddmResidencyHandler>(wddm);
        wddmAllocation.handle = 0x2;
    }

    std::unique_ptr<WddmResidencyHandler> wddmResidencyHandler;
    MockWddmAllocation wddmAllocation;
};

TEST_F(WddmResidencyHandlerTest, whenMakingResidentAllocaionExpectMakeResidentCalled) {
    EXPECT_TRUE(wddmResidencyHandler->makeResident(wddmAllocation));
    EXPECT_TRUE(wddmResidencyHandler->isResident(wddmAllocation));
}

TEST_F(WddmResidencyHandlerTest, whenEvictingResidentAllocationExpectEvictCalled) {
    EXPECT_TRUE(wddmResidencyHandler->makeResident(wddmAllocation));
    EXPECT_TRUE(wddmResidencyHandler->evict(wddmAllocation));
    EXPECT_FALSE(wddmResidencyHandler->isResident(wddmAllocation));
}
