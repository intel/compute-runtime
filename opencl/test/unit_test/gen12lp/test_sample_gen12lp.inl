/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> TigerlakeLpOnlyTest;

HWTEST2_F(TigerlakeLpOnlyTest, WhenGettingHardwareInfoThenProductFamilyIsTigerlakeLp, IsTGLLP) {
    EXPECT_EQ(IGFX_TIGERLAKE_LP, pDevice->getHardwareInfo().platform.eProductFamily);
}

typedef Test<ClDeviceFixture> Gen12LpOnlyTeset;

GEN12LPTEST_F(Gen12LpOnlyTeset, WhenGettingRenderCoreFamilyThenGen12lpCoreIsReturned) {
    EXPECT_NE(IGFX_GEN9_CORE, pDevice->getRenderCoreFamily());
    EXPECT_NE(IGFX_GEN11_CORE, pDevice->getRenderCoreFamily());
    EXPECT_EQ(IGFX_GEN12LP_CORE, pDevice->getRenderCoreFamily());
}
