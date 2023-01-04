/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using ClGfxCoreHelperTestGen11 = Test<ClDeviceFixture>;

GEN11TEST_F(ClGfxCoreHelperTestGen11, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_EQ(ClGfxCoreHelperMock::makeDeviceIpVersion(11, 0, 0), clGfxCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}

GEN11TEST_F(ClGfxCoreHelperTestGen11, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    EXPECT_EQ(0u, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
}
