/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XE_HP_COREOnlyTest;

XE_HP_CORE_TEST_F(XE_HP_COREOnlyTest, WhenGettingRenderCoreFamilyThenOnlyXeHpCoreIsReturned) {
    EXPECT_EQ(IGFX_XE_HP_CORE, pDevice->getRenderCoreFamily());
}
