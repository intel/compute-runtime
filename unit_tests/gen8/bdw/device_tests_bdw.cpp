/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_source_level_debugger.h"
#include "test.h"

using namespace OCLRT;
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
