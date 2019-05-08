/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> BroadwellOnlyTest;

BDWTEST_F(BroadwellOnlyTest, shouldPassOnBdw) {
    EXPECT_EQ(IGFX_BROADWELL, pDevice->getHardwareInfo().platform.eProductFamily);
}

typedef Test<DeviceFixture> Gen8OnlyTest;

GEN8TEST_F(Gen8OnlyTest, shouldPassOnGen8) {
    EXPECT_EQ(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
}
