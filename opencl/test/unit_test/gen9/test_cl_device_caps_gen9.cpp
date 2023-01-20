/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

using Gen9ClDeviceCaps = Test<ClDeviceFixture>;

GEN9TEST_F(Gen9ClDeviceCaps, WhenCheckingExtensionStringThenFp64CorrectlyReported) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    if (pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64) {
        EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_NE(0u, caps.doubleFpConfig);
    } else {
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_EQ(0u, caps.doubleFpConfig);
    }
}

GEN9TEST_F(Gen9ClDeviceCaps, givenGen9WhenCheckExtensionsThenDeviceProperlyReportsClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();
    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    } else {
        EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
    }
}

GEN9TEST_F(Gen9ClDeviceCaps, givenGen9WhenCheckingCapsThenDeviceDoesProperlyReportsIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }
}

GEN9TEST_F(Gen9ClDeviceCaps, WhenGettingDeviceInfoThenCorrectlyRoundedDivideSqrtIsEnabled) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}
