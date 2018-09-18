/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef Test<DeviceFixture> CannonlakeOnlyTest;

CNLTEST_F(CannonlakeOnlyTest, shouldPassOnCnl) {
    EXPECT_EQ(IGFX_CANNONLAKE, pDevice->getHardwareInfo().pPlatform->eProductFamily);
}

typedef Test<DeviceFixture> Gen10OnlyTest;

GEN10TEST_F(Gen10OnlyTest, shouldPassOnGen10) {
    EXPECT_NE(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
    EXPECT_NE(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
    EXPECT_EQ(IGFX_GEN10_CORE, pDevice->getRenderCoreFamily());
}
