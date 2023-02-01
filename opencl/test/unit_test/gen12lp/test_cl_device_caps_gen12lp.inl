/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using Gen12LpClDeviceCaps = Test<ClDeviceFixture>;

HWTEST2_F(Gen12LpClDeviceCaps, WhenCheckingExtensionStringThenFp64IsNotSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

HWTEST2_F(Gen12LpClDeviceCaps, givenGen12lpWhenCheckExtensionsThenSubgroupLocalBlockIOIsSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
}

HWTEST2_F(Gen12LpClDeviceCaps, givenGen12lpWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
}

HWTEST2_F(Gen12LpClDeviceCaps, givenGen12lpWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(caps.independentForwardProgress);
}

HWTEST2_F(Gen12LpClDeviceCaps, WhenCheckingCapsThenCorrectlyRoundedDivideSqrtIsNotSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN12LPTEST_F(Gen12LpClDeviceCaps, WhenCheckingCapsThenProfilingTimerResolutionIs83) {
    const auto &caps = pClDevice->getSharedDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}
