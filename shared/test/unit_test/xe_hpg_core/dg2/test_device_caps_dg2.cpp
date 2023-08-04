/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using Dg2UsDeviceIdTest = Test<DeviceFixture>;

DG2TEST_F(Dg2UsDeviceIdTest, givenDg2ProductWhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenDg2ProductWhenCheckFp64SupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenDg2ProductWhenCheckFp64EmulationSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64Emulation);
}

DG2TEST_F(Dg2UsDeviceIdTest, givenEnabledFtrPooledEuA0SteppingAndG10DevIdWhenCalculatingMaxEuPerSSThenDontIgnoreEuCountPerPoolMin) {
    HardwareInfo myHwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &mySysInfo = myHwInfo.gtSystemInfo;
    FeatureTable &mySkuTable = myHwInfo.featureTable;
    PLATFORM &myPlatform = myHwInfo.platform;

    mySysInfo.EUCount = 20;
    mySysInfo.EuCountPerPoolMin = 99999;
    mySkuTable.flags.ftrPooledEuEnabled = 1;
    myPlatform.usRevId = getHelper<ProductHelper>().getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    myPlatform.usDeviceID = dg2G10DeviceIds[0];

    auto device = MockDevice::createWithNewExecutionEnvironment<MockDevice>(&myHwInfo);

    auto expectedMaxWGS = mySysInfo.EuCountPerPoolMin * (mySysInfo.ThreadCount / mySysInfo.EUCount) * 8;
    expectedMaxWGS = std::min(Math::prevPowerOfTwo(expectedMaxWGS), 512u);

    EXPECT_EQ(expectedMaxWGS, device->getDeviceInfo().maxWorkGroupSize);

    delete device;
}

DG2TEST_F(Dg2UsDeviceIdTest, givenRevisionEnumThenProperMaxThreadsForWorkgroupIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    hardwareInfo.platform.usDeviceID = dg2G10DeviceIds[0];
    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    EXPECT_EQ(64u, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));

    hardwareInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A1, hardwareInfo);
    uint32_t numThreadsPerEU = hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount;
    EXPECT_EQ(64u * numThreadsPerEU, productHelper.getMaxThreadsForWorkgroupInDSSOrSS(hardwareInfo, 64u, 64u));
}
