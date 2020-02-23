/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen9OnlyTest;

GEN9TEST_F(Gen9OnlyTest, shouldPassOnGen9) {
    EXPECT_NE(IGFX_GEN8_CORE, pDevice->getRenderCoreFamily());
    EXPECT_EQ(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
}
