/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds_bxt.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> DeviceTest;

BXTTEST_F(DeviceTest, givenBxtDeviceWhenAskedForProflingTimerResolutionThen52IsReturned) {
    auto resolution = pDevice->getProfilingTimerResolution();
    EXPECT_DOUBLE_EQ(52.083, resolution);
}
