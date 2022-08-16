/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> Gen9OnlyTest;

GEN9TEST_F(Gen9OnlyTest, WhenGettingRenderCoreFamilyThenGen9CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
}
