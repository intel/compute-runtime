/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> SkylakeOnlyTest;

SKLTEST_F(SkylakeOnlyTest, WhenGettingProductFamilyThenSkylakeIsReturned) {
    EXPECT_EQ(IGFX_SKYLAKE, pDevice->getHardwareInfo().platform.eProductFamily);
}
