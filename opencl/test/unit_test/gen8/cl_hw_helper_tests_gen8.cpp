/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using namespace NEO;
using ClHwHelperTestGen8 = ::testing::Test;

GEN8TEST_F(ClHwHelperTestGen8, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(8, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN8TEST_F(ClHwHelperTestGen8, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities(*defaultHwInfo));
}
