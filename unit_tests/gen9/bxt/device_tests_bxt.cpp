/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef Test<DeviceFixture> DeviceTest;

BXTTEST_F(DeviceTest, getSupportedClVersion12Device) {
    auto version = pDevice->getSupportedClVersion();
    EXPECT_EQ(12u, version);
}

BXTTEST_F(DeviceTest, givenBxtDeviceWhenAskedForProflingTimerResolutionThen52IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(52.083, resolution);
}
