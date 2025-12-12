/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

using namespace NEO;

using Xe3pCoreOnlyTest = Test<ClDeviceFixture>;

XE3P_CORETEST_F(Xe3pCoreOnlyTest, WhenGettingRenderCoreFamilyThenOnlyXe3pCoreIsReturned) {
    EXPECT_EQ(NEO::xe3pCoreEnumValue, pDevice->getRenderCoreFamily());
}
