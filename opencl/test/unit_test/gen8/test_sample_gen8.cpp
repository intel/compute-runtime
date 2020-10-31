/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<ClDeviceFixture> BroadwellOnlyTest;

BDWTEST_F(BroadwellOnlyTest, WhenGettingProductFamilyThenBroadwellIsReturned) {
    EXPECT_EQ(IGFX_BROADWELL, pDevice->getHardwareInfo().platform.eProductFamily);
}

typedef Test<ClDeviceFixture> Gen8OnlyTest;

GEN8TEST_F(Gen8OnlyTest, WhenGettingRenderCoreFamilyThenGen8CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
}
