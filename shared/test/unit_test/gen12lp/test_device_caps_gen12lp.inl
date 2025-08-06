/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen12LpDeviceCaps = Test<DeviceFixture>;

GEN12LPTEST_F(Gen12LpDeviceCaps, GivenDefaultWhenCheckingPreemptionModeThenMidThreadIsReported) {
    EXPECT_EQ(PreemptionMode::ThreadGroup, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, WhenCheckingCapsThenKmdNotifyMechanismIsCorrectlyReported) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, WhenCheckingCapsThenCompressionIsDisabled) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &gfxCoreHelperl = getHelper<GfxCoreHelper>();

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * 8;

    EXPECT_EQ(expectedValue, gfxCoreHelperl.getComputeUnitsUsedForScratch(pDevice->getRootDeviceEnvironment()));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.maxProgrammableSlmSize);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckingMediaBlockSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckingCoherencySupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsCoherency);
}

HWTEST2_F(Gen12LpDeviceCaps, givenTglLpWhenCheckSupportCacheFlushAfterWalkerThenFalse, IsTGLLP) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

HWTEST2_F(Gen12LpDeviceCaps, givenGen12lpWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue, IsTGLLP) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckingFloatAtomicsSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsFloatAtomics);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpWhenCheckingCxlTypeThenReturnZero) {
    EXPECT_EQ(0u, pDevice->getHardwareInfo().capabilityTable.cxlType);
}
