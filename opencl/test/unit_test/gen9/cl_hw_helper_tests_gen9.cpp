/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestGen9 = HwHelperTest;

GEN9TEST_F(HwHelperTestGen9, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(9, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN9TEST_F(HwHelperTestGen9, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities(*defaultHwInfo));
}
