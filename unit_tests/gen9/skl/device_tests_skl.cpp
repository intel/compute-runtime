/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace OCLRT;

typedef Test<DeviceFixture> DeviceTest;

SKLTEST_F(DeviceTest, getSupportedClVersion21Device) {
    auto version = pDevice->getSupportedClVersion();
    EXPECT_EQ(21u, version);
}

SKLTEST_F(DeviceTest, givenSklDeviceWhenAskedForProflingTimerResolutionThen83IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(83.333, resolution);
}
