/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"
#include "test.h"
#include "unit_tests/fixtures/device_fixture.h"

using namespace NEO;

typedef Test<DeviceFixture> Gen10DeviceCaps;

GEN10TEST_F(Gen10DeviceCaps, reportsOcl21) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

GEN10TEST_F(Gen10DeviceCaps, allSkusSupportFP64) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_NE(0u, caps.doubleFpConfig);
}

GEN10TEST_F(Gen10DeviceCaps, allSkusSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN10TEST_F(Gen10DeviceCaps, defaultPreemptionMode) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN10TEST_F(Gen10DeviceCaps, whitelistedRegisters) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.whitelistedRegisters.csChicken1_0x2580);
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.whitelistedRegisters.chicken0hdc_0xE5F0);
}

GEN10TEST_F(Gen10DeviceCaps, profilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(83u, caps.outProfilingTimerResolution);
}

GEN10TEST_F(Gen10DeviceCaps, compression) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN10TEST_F(Gen10DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.pSysInfo->MaxSubSlicesSupported * hwInfo.pSysInfo->MaxEuPerSubSlice *
                             hwInfo.pSysInfo->ThreadCount / hwInfo.pSysInfo->EUCount;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN10TEST_F(Gen10DeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

GEN10TEST_F(Gen10DeviceCaps, givenGen10WhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}

typedef Test<DeviceFixture> CnlUsDeviceIdTest;

CNLTEST_F(CnlUsDeviceIdTest, isSimulationCap) {
    unsigned short cnlSimulationIds[6] = {
        ICNL_3x8_DESK_DEVICE_F0_ID,
        ICNL_5x8_DESK_DEVICE_F0_ID,
        ICNL_9x8_DESK_DEVICE_F0_ID,
        ICNL_13x8_DESK_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    NEO::MockDevice *mockDevice = nullptr;

    for (auto id : cnlSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

CNLTEST_F(CnlUsDeviceIdTest, kmdNotifyMechanism) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableKmdNotify);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleep);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    EXPECT_EQ(0, pDevice->getHardwareInfo().capabilityTable.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
}

CNLTEST_F(CnlUsDeviceIdTest, GivenCNLWhenCheckftr64KBpagesThenTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}
