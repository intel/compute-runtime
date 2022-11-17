/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using ClHwHelperTestGen9 = Test<ClDeviceFixture>;

GEN9TEST_F(ClHwHelperTestGen9, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(9, 0, 0), clCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}

GEN9TEST_F(ClHwHelperTestGen9, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(0u, clCoreHelper.getSupportedDeviceFeatureCapabilities(*defaultHwInfo));
}
