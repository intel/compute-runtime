/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using ClGfxCoreHelperTestXeHpAndLater = Test<ClDeviceFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, ClGfxCoreHelperTestXeHpAndLater, givenCLImageFormatsWhenCallingIsFormatRedescribableThenFalseIsReturned) {
    static const cl_image_format redescribeFormats[] = {
        {CL_R, CL_UNSIGNED_INT8},
        {CL_R, CL_UNSIGNED_INT16},
        {CL_R, CL_UNSIGNED_INT32},
        {CL_RG, CL_UNSIGNED_INT32},
        {CL_RGBA, CL_UNSIGNED_INT32},
    };

    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    for (const auto &format : redescribeFormats) {
        EXPECT_EQ(false, clGfxCoreHelper.isFormatRedescribable(format));
    }
}

HWTEST2_F(ClGfxCoreHelperTestXeHpAndLater, WhenGettingSupportedDeviceFeatureCapabilitiesThenReturnCorrectValue, IsAtLeastXeCore) {
    auto &clGfxCoreHelper = getHelper<ClGfxCoreHelper>();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper && !releaseHelper->isMatrixMultiplyAccumulateSupported()) {
        cl_device_feature_capabilities_intel expectedCapabilities = CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
        EXPECT_EQ(expectedCapabilities, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
    } else {
        cl_device_feature_capabilities_intel expectedCapabilities = CL_DEVICE_FEATURE_FLAG_DPAS_INTEL | CL_DEVICE_FEATURE_FLAG_DP4A_INTEL;
        EXPECT_EQ(expectedCapabilities, clGfxCoreHelper.getSupportedDeviceFeatureCapabilities(getRootDeviceEnvironment()));
    }
}
