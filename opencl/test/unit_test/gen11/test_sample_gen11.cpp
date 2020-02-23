/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen11OnlyTeset;

GEN11TEST_F(Gen11OnlyTeset, shouldPassOnGen11) {
    EXPECT_NE(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
    EXPECT_EQ(IGFX_GEN11_CORE, pDevice->getRenderCoreFamily());
}
