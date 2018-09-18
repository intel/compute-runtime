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

SKLTEST_F(DeviceTest, getSupportedClVersion21Device) {
    auto version = pDevice->getSupportedClVersion();
    EXPECT_EQ(21u, version);
}

SKLTEST_F(DeviceTest, givenSklDeviceWhenAskedForProflingTimerResolutionThen83IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(83.333, resolution);
}
