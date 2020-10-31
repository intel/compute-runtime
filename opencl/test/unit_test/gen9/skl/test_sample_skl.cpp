/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<ClDeviceFixture> SkylakeOnlyTest;

SKLTEST_F(SkylakeOnlyTest, WhenGettingProductFamilyThenSkylakeIsReturned) {
    EXPECT_EQ(IGFX_SKYLAKE, pDevice->getHardwareInfo().platform.eProductFamily);
}
