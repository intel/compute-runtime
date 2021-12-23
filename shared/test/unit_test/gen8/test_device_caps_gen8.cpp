/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8DeviceCaps = Test<DeviceFixture>;

GEN8TEST_F(Gen8DeviceCaps, GivenDefaultWhenCheckingPreemptionModeThenDisabledIsReported) {
    EXPECT_TRUE(PreemptionMode::Disabled == pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN8TEST_F(Gen8DeviceCaps, BdwProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(80u, caps.outProfilingTimerResolution);
}

GEN8TEST_F(Gen8DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice *
                             hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN8TEST_F(Gen8DeviceCaps, givenHwInfoWhenRequestedMaxFrontEndThreadsThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();

    EXPECT_EQ(HwHelper::getMaxThreadsForVfe(hwInfo), pDevice->getDeviceInfo().maxFrontEndThreads);
}

GEN8TEST_F(Gen8DeviceCaps, GivenBdwWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(defaultHwInfo->capabilityTable.ftr64KBpages);
}

GEN8TEST_F(Gen8DeviceCaps, GivenDefaultSettingsWhenCheckingPreemptionModeThenPreemptionIsDisabled) {
    EXPECT_TRUE(PreemptionMode::Disabled == pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN8TEST_F(Gen8DeviceCaps, WhenCheckingKmdNotifyMechanismThenPropertiesAreSetCorrectly) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(50000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(5000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(200000, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

GEN8TEST_F(Gen8DeviceCaps, WhenCheckingCompressionThenItIsDisabled) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN8TEST_F(Gen8DeviceCaps, WhenCheckingImage3dDimensionsThenCapsAreSetCorrectly) {
    const auto &sharedCaps = pDevice->getDeviceInfo();
    EXPECT_EQ(2048u, sharedCaps.image3DMaxDepth);
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

GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckingMediaBlockSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}
GEN8TEST_F(Gen8DeviceCaps, givenGen8WhenCheckingDeviceEnqueueSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsDeviceEnqueue);
}
