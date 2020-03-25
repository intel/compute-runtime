/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen8DeviceCaps;

GEN8TEST_F(Gen8DeviceCaps, defaultPreemptionMode) {
    EXPECT_TRUE(PreemptionMode::Disabled == pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckExtensionsThenDeviceProperlyReportsClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();
    if (pClDevice->getEnabledClVersion() >= 21) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_subgroups")));
    } else {
        EXPECT_THAT(caps.deviceExtensions, ::testing::Not(testing::HasSubstr(std::string("cl_khr_subgroups"))));
    }
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckingCapsThenDeviceDoesProperlyReportsIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->getEnabledClVersion() >= 21) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }
}

GEN8TEST_F(Gen8DeviceCaps, kmdNotifyMechanism) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

GEN8TEST_F(Gen8DeviceCaps, compression) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN8TEST_F(Gen8DeviceCaps, image3DDimensions) {
    const auto &caps = pClDevice->getDeviceInfo();
    const auto &sharedCaps = pDevice->getDeviceInfo();
    EXPECT_EQ(2048u, caps.image3DMaxWidth);
    EXPECT_EQ(2048u, sharedCaps.image3DMaxDepth);
    EXPECT_EQ(2048u, caps.image3DMaxHeight);
}

GEN8TEST_F(Gen8DeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}
