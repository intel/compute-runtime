/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> IcelakeLpOnlyTest;

ICLLPTEST_F(IcelakeLpOnlyTest, shouldPassOnIcllp) {
    EXPECT_EQ(IGFX_ICELAKE_LP, pDevice->getHardwareInfo().platform.eProductFamily);
}

typedef Test<DeviceFixture> Gen11OnlyTeset;

GEN11TEST_F(Gen11OnlyTeset, shouldPassOnGen11) {
    EXPECT_NE(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
    EXPECT_EQ(IGFX_GEN11_CORE, pDevice->getRenderCoreFamily());
}
