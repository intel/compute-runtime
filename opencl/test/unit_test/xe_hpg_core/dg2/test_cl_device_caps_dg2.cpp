/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/ult_cl_device_factory.h"

using namespace NEO;

using Dg2UsDeviceIdTest = Test<ClDeviceFixture>;

DG2TEST_F(Dg2UsDeviceIdTest, givenDeviceThatHasHighNumberOfExecutionUnitsA0SteppingAndG10DevIdWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo512) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * 8;
    myPlatform.usRevId = getHelper<ProductHelper>().getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    myPlatform.usDeviceID = dg2G10DeviceIds[0];
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(512u, device->sharedDeviceInfo.maxWorkGroupSize);
    EXPECT_EQ(device->sharedDeviceInfo.maxWorkGroupSize / 8, device->getDeviceInfo().maxNumOfSubGroups);
}

using Dg2DeviceCapsTest = ::testing::Test;

DG2TEST_F(Dg2DeviceCapsTest, whenCheckingExtensionThenCorrectExtensionsAreReported) {
    UltClDeviceFactory deviceFactory{1, 0};
    auto &extensions = deviceFactory.rootDevices[0]->deviceExtensions;

    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_bfloat16_conversions")));
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));
}
