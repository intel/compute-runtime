/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> Gen12LpDeviceCaps;

HWTEST2_F(Gen12LpDeviceCaps, WhenCheckingExtensionStringThenFp64IsNotSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

HWTEST2_F(Gen12LpDeviceCaps, givenGen12lpWhenCheckExtensionsThenSubgroupLocalBlockIOIsSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_TRUE(hasSubstr(caps.deviceExtensions, std::string("cl_intel_subgroup_local_block_io")));
}

HWTEST2_F(Gen12LpDeviceCaps, givenGen12lpWhenCheckExtensionsThenDeviceDoesNotReportClKhrSubgroupsExtension, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(hasSubstr(caps.deviceExtensions, std::string("cl_khr_subgroups")));
}

HWTEST2_F(Gen12LpDeviceCaps, givenGen12lpWhenCheckingCapsThenDeviceDoesNotSupportIndependentForwardProgress, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();

    EXPECT_FALSE(caps.independentForwardProgress);
}

HWTEST2_F(Gen12LpDeviceCaps, WhenCheckingCapsThenCorrectlyRoundedDivideSqrtIsNotSupported, IsTGLLP) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, GivenDefaultWhenCheckingPreemptionModeThenMidThreadIsReported) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, WhenCheckingCapsThenProfilingTimerResolutionIs83) {
    const auto &caps = pClDevice->getSharedDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
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
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * 8;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
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

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpDeviceWhenCheckingDeviceEnqueueSupportThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsDeviceEnqueue);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, givenGen12LpDeviceWhenCheckingPipesSupportThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportsPipes);
}

using TglLpUsDeviceIdTest = Test<ClDeviceFixture>;

HWTEST2_F(TglLpUsDeviceIdTest, WhenCheckingSimulationCapThenResultIsCorrect, IsTGLLP) {
    unsigned short tglLpSimulationIds[2] = {
        0xFF20,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : tglLpSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

HWTEST2_F(TglLpUsDeviceIdTest, GivenTGLLPWhenCheckftr64KBpagesThenTrue, IsTGLLP) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

HWTEST2_F(TglLpUsDeviceIdTest, givenGen12lpWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue, IsTGLLP) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}
