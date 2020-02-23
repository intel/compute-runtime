/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_source_level_debugger.h"
#include "test.h"

using namespace NEO;
struct BdwDeviceTest : public DeviceFixture,
                       public ::testing::Test {
    void SetUp() override {
        DeviceFixture::SetUp();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }
};

BDWTEST_F(BdwDeviceTest, givenBdwDeviceWhenAskedForClVersionThenReport21) {
    auto version = pDevice->getSupportedClVersion();
    EXPECT_EQ(21u, version);
}
