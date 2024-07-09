/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using Xe2HpgCoreOnlyTest = Test<ClDeviceFixture>;

XE2_HPG_CORETEST_F(Xe2HpgCoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyHpgCoreIsReturned) {
    EXPECT_EQ(IGFX_XE2_HPG_CORE, pDevice->getRenderCoreFamily());
}
