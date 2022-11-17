/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using namespace NEO;
using ClHwHelperTestGen8 = Test<ClDeviceFixture>;

GEN8TEST_F(ClHwHelperTestGen8, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(8, 0, 0), clCoreHelper.getDeviceIpVersion(*defaultHwInfo));
}

GEN8TEST_F(ClHwHelperTestGen8, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    auto &clCoreHelper = getHelper<ClCoreHelper>();
    EXPECT_EQ(0u, clCoreHelper.getSupportedDeviceFeatureCapabilities(*defaultHwInfo));
}
