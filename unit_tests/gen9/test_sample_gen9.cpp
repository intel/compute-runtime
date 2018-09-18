/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef Test<DeviceFixture> Gen9OnlyTest;

GEN9TEST_F(Gen9OnlyTest, shouldPassOnGen9) {
    EXPECT_EQ(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
}
