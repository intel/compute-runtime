/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> Gen11OnlyTeset;

GEN11TEST_F(Gen11OnlyTeset, WhenGettingRenderCoreFamilyThenGen11CoreIsReturned) {
    EXPECT_EQ(IGFX_GEN11_CORE, pDevice->getRenderCoreFamily());
}
