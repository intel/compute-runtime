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
#include "opencl/test/unit_test/mocks/mock_cl_hw_helper.h"

using HwHelperTestGen11 = HwHelperTest;

GEN11TEST_F(HwHelperTestGen11, WhenGettingDeviceIpVersionThenMakeCorrectDeviceIpVersion) {
    EXPECT_EQ(ClHwHelperMock::makeDeviceIpVersion(11, 0, 0), ClHwHelper::get(renderCoreFamily).getDeviceIpVersion(*defaultHwInfo));
}

GEN11TEST_F(HwHelperTestGen11, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue) {
    EXPECT_EQ(0u, ClHwHelper::get(renderCoreFamily).getSupportedDeviceFeatureCapabilities(hardwareInfo));
}
