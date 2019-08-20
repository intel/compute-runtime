/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen11DeviceCaps;

LKFTEST_F(Gen11DeviceCaps, givenLKFWhenCheckedOCLVersionThen21IsReported) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 1.2 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 1.2 ", caps.clCVersion);
}

LKFTEST_F(Gen11DeviceCaps, givenLKFWhenCheckedSvmSupportThenNoSvmIsReported) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(caps.svmCapabilities, 0u);
}

LKFTEST_F(Gen11DeviceCaps, givenLkfWhenDoublePrecissionIsCheckedThenFalseIsReturned) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrSupports64BitMath);
}

LKFTEST_F(Gen11DeviceCaps, givenLkfWhenExtensionStringIsCheckedThenFP64IsNotReported) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

LKFTEST_F(Gen11DeviceCaps, givenLkfWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

ICLLPTEST_F(Gen11DeviceCaps, lpSkusDontSupportFP64) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

ICLLPTEST_F(Gen11DeviceCaps, lpSkusDontSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

ICLLPTEST_F(Gen11DeviceCaps, givenIclLpWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

GEN11TEST_F(Gen11DeviceCaps, defaultPreemptionMode) {
    EXPECT_TRUE(PreemptionMode::MidThread == pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN11TEST_F(Gen11DeviceCaps, profilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

GEN11TEST_F(Gen11DeviceCaps, kmdNotifyMechanism) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

GEN11TEST_F(Gen11DeviceCaps, compression) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN11TEST_F(Gen11DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice * 8;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN11TEST_F(Gen11DeviceCaps, givenHwInfoWhenRequestedMaxFrontEndThreadsThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();

    EXPECT_EQ(HwHelper::getMaxThreadsForVfe(hwInfo), pDevice->getDeviceInfo().maxFrontEndThreads);
}

GEN11TEST_F(Gen11DeviceCaps, givenGen11WhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

GEN11TEST_F(Gen11DeviceCaps, givenGen11WhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

typedef Test<DeviceFixture> IclLpUsDeviceIdTest;

ICLLPTEST_F(IclLpUsDeviceIdTest, isSimulationCap) {
    unsigned short iclLpSimulationIds[2] = {
        IICL_LP_GT1_MOB_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : iclLpSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

ICLLPTEST_F(IclLpUsDeviceIdTest, GivenICLLPWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

typedef Test<DeviceFixture> LkfUsDeviceIdTest;

LKFTEST_F(LkfUsDeviceIdTest, isSimulationCap) {
    unsigned short lkfSimulationIds[2] = {
        ILKF_1x8x8_DESK_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : lkfSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}
