/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XeHpgCoreOnlyTeset;

XE_HPG_CORETEST_F(XeHpgCoreOnlyTeset, WhenGettingRenderCoreFamilyThenXeHpgCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HPG_CORE, pDevice->getRenderCoreFamily());
}
