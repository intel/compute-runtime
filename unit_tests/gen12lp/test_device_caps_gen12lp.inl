/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen12LpDeviceCaps;

GEN12LPTEST_F(Gen12LpDeviceCaps, reportsOcl21) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

TGLLPTEST_F(Gen12LpDeviceCaps, lpSkusDontSupportFP64) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_EQ(0u, caps.doubleFpConfig);
}

TGLLPTEST_F(Gen12LpDeviceCaps, givenGen12lpWhenCheckExtensionsThenSubgroupLocalBlockIOIsSupported) {
    const auto &caps = pDevice->getDeviceInfo();

    EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_intel_subgroup_local_block_io")));
}

TGLLPTEST_F(Gen12LpDeviceCaps, allSkusSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, defaultPreemptionMode) {
    EXPECT_EQ(PreemptionMode::ThreadGroup, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, profilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, kmdNotifyMechanism) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

GEN12LPTEST_F(Gen12LpDeviceCaps, compression) {
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

TGLLPTEST_F(Gen12LpDeviceCaps, givenTglLpWhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

using TglLpUsDeviceIdTest = Test<DeviceFixture>;

TGLLPTEST_F(TglLpUsDeviceIdTest, isSimulationCap) {
    unsigned short tglLpSimulationIds[2] = {
        IGEN12LP_GT1_MOB_DEVICE_F0_ID,
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

TGLLPTEST_F(TglLpUsDeviceIdTest, GivenTGLLPWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}

TGLLPTEST_F(TglLpUsDeviceIdTest, givenGen12lpWhenCheckFtrSupportsInteger64BitAtomicsThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsInteger64BitAtomics);
}
