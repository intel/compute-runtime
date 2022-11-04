/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_hw_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

using ClHwHelperTestXeHpAndLater = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, ClHwHelperTestXeHpAndLater, givenCLImageFormatsWhenCallingIsFormatRedescribableThenFalseIsReturned) {
    static const cl_image_format redescribeFormats[] = {
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT32},
    };
    MockContext context;
    auto &clHwHelper = ClHwHelper::get(context.getDevice(0)->getHardwareInfo().platform.eRenderCoreFamily);

    for (const auto &format : redescribeFormats) {
        EXPECT_EQ(false, clHwHelper.isFormatRedescribable(format));
    }
}
