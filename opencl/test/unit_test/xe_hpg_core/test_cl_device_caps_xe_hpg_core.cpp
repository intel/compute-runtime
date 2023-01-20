/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

#include "hw_cmds_xe_hpg_core_base.h"

using namespace NEO;

using XeHpgCoreClDeviceCaps = Test<ClDeviceFixture>;

XE_HPG_CORETEST_F(XeHpgCoreClDeviceCaps, givenXeHpgCoreWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
}

XE_HPG_CORETEST_F(XeHpgCoreClDeviceCaps, giveDeviceExtensionsWhenDeviceCapsInitializedThenAddProperExtensions) {
    const auto &productHelper = getHelper<ProductHelper>();
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_dot_accumulate")));
    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));

    bool expectMatrixMultiplyAccumulateExtensions = productHelper.isMatrixMultiplyAccumulateSupported(pClDevice->getHardwareInfo());
    EXPECT_EQ(expectMatrixMultiplyAccumulateExtensions, hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
    EXPECT_EQ(expectMatrixMultiplyAccumulateExtensions, hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_split_matrix_multiply_accumulate")));

    bool expectBFloat16ConversionsExtension = productHelper.isBFloat16ConversionSupported(pClDevice->getHardwareInfo());
    EXPECT_EQ(expectBFloat16ConversionsExtension, hasSubstr(caps.deviceExtensions, std::string("cl_intel_bfloat16_conversions")));
}

XE_HPG_CORETEST_F(XeHpgCoreClDeviceCaps, givenXeHpgCoreWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(caps.independentForwardProgress);
}

XE_HPG_CORETEST_F(XeHpgCoreClDeviceCaps, givenDeviceThatHasHighNumberOfExecutionUnitsAndNotA0SteppingWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo1024) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * 8; // 128 threads per subslice, in simd 8 gives 1024
    myPlatform.usRevId = 0x4;
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(1024u, device->sharedDeviceInfo.maxWorkGroupSize);
    EXPECT_EQ(device->sharedDeviceInfo.maxWorkGroupSize / 8, device->getDeviceInfo().maxNumOfSubGroups);
}
