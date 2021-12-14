/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

#include "gmock/gmock.h"

using namespace NEO;

using Dg2UsDeviceIdTest = Test<ClDeviceFixture>;

DG2TEST_F(Dg2UsDeviceIdTest, givenDg2ProductWhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenDg2ProductWhenCheckFp64SupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenDeviceThatHasHighNumberOfExecutionUnitsAndA0SteppingWhenMaxWorkgroupSizeIsComputedThenItIsLimitedTo512) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    mySysInfo.EUCount = 32;
    mySysInfo.SubSliceCount = 2;
    mySysInfo.DualSubSliceCount = 2;
    mySysInfo.ThreadCount = 32 * 8;
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    EXPECT_EQ(512u, device->sharedDeviceInfo.maxWorkGroupSize);
    EXPECT_EQ(device->sharedDeviceInfo.maxWorkGroupSize / 8, device->getDeviceInfo().maxNumOfSubGroups);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenEnabledFtrPooledEuAndA0SteppingWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;
    PLATFORM &myPlatform = myHwInfo.platform;
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;
    myPlatform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);

    auto device = std::unique_ptr<Device>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo));

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 512u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hardwareInfo.platform.eProductFamily);
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    EXPECT_EQ(64u, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));

    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, hwInfoConfig.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}
