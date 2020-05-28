/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"
#include "test.h"

using namespace NEO;
struct BdwDeviceTest : public ClDeviceFixture,
                       public ::testing::Test {
    void SetUp() override {
        ClDeviceFixture::SetUp();
    }

    void TearDown() override {
        ClDeviceFixture::TearDown();
    }
};

BDWTEST_F(BdwDeviceTest, givenBdwDeviceWhenAskedForClVersionThenReport21) {
    auto version = pClDevice->getEnabledClVersion();
    EXPECT_EQ(21u, version);
}
