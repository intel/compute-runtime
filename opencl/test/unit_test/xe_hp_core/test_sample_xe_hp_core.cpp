/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XE_HP_COREOnlyTest;

XE_HP_CORE_TEST_F(XE_HP_COREOnlyTest, WhenGettingRenderCoreFamilyThenOnlyXeHpCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HP_CORE, pDevice->getRenderCoreFamily());
}
