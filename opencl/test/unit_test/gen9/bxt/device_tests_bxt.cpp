/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<ClDeviceFixture> DeviceTest;

BXTTEST_F(DeviceTest, getEnabledClVersion12Device) {
    auto version = pClDevice->getEnabledClVersion();
    EXPECT_EQ(12u, version);
}

BXTTEST_F(DeviceTest, givenBxtDeviceWhenAskedForProflingTimerResolutionThen52IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(52.083, resolution);
}
