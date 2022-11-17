/*
 * Copyright (C) 2019-2022 Intel Corporation
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

using ClHwHelperTestGen11 = Test<ClDeviceFixture>;

GEN11TEST_F(ClHwHelperTestGen11, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(11, 0, 0), clCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}

GEN11TEST_F(ClHwHelperTestGen11, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(0u, clCoreHelper.getSupportedDeviceFeatureCapabilities(*defaultHwInfo));
}
