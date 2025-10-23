/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

using namespace NEO;

using XeHpgCoreDeviceCaps = Test<DeviceFixture>;

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingMediaBlockSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, givenXeHpgCoreWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
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

XE_HPG_CORETEST_F(XeHpgCoreDeviceCaps, GivenVariousValuesWhenComputeSlmSizeIsCalledThenCorrectValueIsReturned) {
    struct ComputeSlmTestInput {
        uint32_t expected;
        uint32_t slmSize;
    };

    ComputeSlmTestInput computeSlmValuesXeHpgTestsInput[] = {
        {0, 0 * MemoryConstants::kiloByte},
        {1, 0 * MemoryConstants::kiloByte + 1},
        {1, 1 * MemoryConstants::kiloByte},
        {2, 1 * MemoryConstants::kiloByte + 1},
        {2, 2 * MemoryConstants::kiloByte},
        {3, 2 * MemoryConstants::kiloByte + 1},
        {3, 4 * MemoryConstants::kiloByte},
        {4, 4 * MemoryConstants::kiloByte + 1},
        {4, 8 * MemoryConstants::kiloByte},
        {5, 8 * MemoryConstants::kiloByte + 1},
        {5, 16 * MemoryConstants::kiloByte},
        {6, 16 * MemoryConstants::kiloByte + 1},
        {6, 32 * MemoryConstants::kiloByte},
        {7, 32 * MemoryConstants::kiloByte + 1},
        {7, 64 * MemoryConstants::kiloByte}};

    auto hardwareInfo = *defaultHwInfo;
    for (auto &testInput : computeSlmValuesXeHpgTestsInput) {
        EXPECT_EQ(testInput.expected, EncodeDispatchKernel<FamilyType>::computeSlmValues(hardwareInfo, testInput.slmSize, nullptr, false));
    }
}