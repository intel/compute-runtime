/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

using Gen8ClDeviceCaps = Test<ClDeviceFixture>;

GEN8TEST_F(Gen8ClDeviceCaps, WhenCheckingExtensionStringThenFp64IsSupported) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_NE(0u, caps.doubleFpConfig);
}

GEN8TEST_F(Gen8ClDeviceCaps, WhenGettingDeviceInfoThenCorrectlyRoundedDivideSqrtIsEnabled) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN8TEST_F(Gen8ClDeviceCaps, givenGen8WhenCheckExtensionsThenDeviceProperlyReportsClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();
    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    }
}

GEN8TEST_F(Gen8ClDeviceCaps, givenGen8WhenCheckingCapsThenDeviceDoesProperlyReportsIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }
}

GEN8TEST_F(Gen8ClDeviceCaps, WhenCheckingImage3dDimensionsThenCapsAreSetCorrectly) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(2048u, caps.image3DMaxWidth);
    EXPECT_EQ(2048u, caps.image3DMaxHeight);
}
