/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> BroadwellOnlyTest;

BDWTEST_F(BroadwellOnlyTest, shouldPassOnBdw) {
    EXPECT_EQ(IGFX_BROADWELL, pDevice->getHardwareInfo().platform.eProductFamily);
}

typedef Test<DeviceFixture> Gen8OnlyTest;

GEN8TEST_F(Gen8OnlyTest, shouldPassOnGen8) {
    EXPECT_EQ(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
    EXPECT_NE(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
}
