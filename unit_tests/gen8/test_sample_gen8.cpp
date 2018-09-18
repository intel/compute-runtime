/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef Test<DeviceFixture> BroadwellOnlyTest;

BDWTEST_F(BroadwellOnlyTest, shouldPassOnBdw) {
    EXPECT_EQ(IGFX_BROADWELL, pDevice->getHardwareInfo().pPlatform->eProductFamily);
}

typedef Test<DeviceFixture> Gen8OnlyTest;

GEN8TEST_F(Gen8OnlyTest, shouldPassOnGen8) {
    EXPECT_EQ(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
}
