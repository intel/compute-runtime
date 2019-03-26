/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"

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
