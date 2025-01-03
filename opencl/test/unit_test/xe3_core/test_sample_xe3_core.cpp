/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using Xe3CoreOnlyTest = Test<ClDeviceFixture>;

XE3_CORETEST_F(Xe3CoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyXe3CoreIsReturned) {
    EXPECT_EQ(IGFX_XE3_CORE, pDevice->getRenderCoreFamily());
}
