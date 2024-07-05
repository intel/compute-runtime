/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_gfx_core_helper.h"

using ClGfxCoreHelperTestGen9 = Test<ClDeviceFixture>;

GEN9TEST_F(ClGfxCoreHelperTestGen9, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_EQ(0u, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
}
