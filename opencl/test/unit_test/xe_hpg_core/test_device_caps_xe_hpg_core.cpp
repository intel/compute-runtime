/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> XeHpgCoreDeviceCaps;

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingMediaBlockSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, ::testing::Not(testing::HasSubstr(std::string("cl_khr_subgroups"))));
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, giveDeviceExtensionsWhenDeviceCapsInitializedThenAddProperExtensions) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_create_buffer_with_properties")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_dot_accumulate")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_local_block_io")));
    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_matrix_multiply_accumulate")));
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(caps.independentForwardProgress);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenEnabledFtrPooledEuAndNotA0SteppingWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;
    myPlatform.usRevId = 0x4;

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 1024u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenDeviceThatHasHighNumberOfExecutionUnitsAndNotA0SteppingWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo1024) {
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

HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenEnabledFtrPooledEuWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin, IGFX_XE_HPG_CORE);
HWTEST_EXCLUDE_PRODUCT(DeviceGetCapsTest, givenDeviceThatHasHighNumberOfExecutionUnitsWhenMaxWorkgroupSizeIsComputedItIsLimitedTo1024, IGFX_XE_HPG_CORE);
