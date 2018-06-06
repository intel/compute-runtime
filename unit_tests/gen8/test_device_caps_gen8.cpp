/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/hw_helper.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "test.h"

using namespace OCLRT;

typedef Test<DeviceFixture> Gen8DeviceCaps;

GEN8TEST_F(Gen8DeviceCaps, givenGen8DeviceWhenAskedForClVersionThenReport21) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_STREQ("OpenCL 2.1 NEO ", caps.clVersion);
    EXPECT_STREQ("OpenCL C 2.0 ", caps.clCVersion);
}

GEN8TEST_F(Gen8DeviceCaps, skuSpecificCaps) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;

    EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
    EXPECT_NE(0u, caps.doubleFpConfig);
}

GEN8TEST_F(Gen8DeviceCaps, allSkusSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN8TEST_F(Gen8DeviceCaps, defaultPreemptionMode) {
    EXPECT_TRUE(PreemptionMode::Disabled == pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN8TEST_F(Gen8DeviceCaps, whitelistedRegister) {
    EXPECT_FALSE(pDevice->getWhitelistedRegisters().csChicken1_0x2580);
    EXPECT_FALSE(pDevice->getWhitelistedRegisters().chicken0hdc_0xE5F0);
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
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrCompression);
}

GEN8TEST_F(Gen8DeviceCaps, image3DDimensions) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(2048u, caps.image3DMaxWidth);
    EXPECT_EQ(2048u, caps.image3DMaxDepth);
    EXPECT_EQ(2048u, caps.image3DMaxHeight);
}

BDWTEST_F(Gen8DeviceCaps, BdwProfilingTimerResolution) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_EQ(80u, caps.outProfilingTimerResolution);
}

BDWTEST_F(Gen8DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.pSysInfo->MaxSubSlicesSupported * hwInfo.pSysInfo->MaxEuPerSubSlice *
                             hwInfo.pSysInfo->ThreadCount / hwInfo.pSysInfo->EUCount;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

typedef Test<DeviceFixture> BdwUsDeviceIdTest;

BDWTEST_F(BdwUsDeviceIdTest, isSimulationCap) {
    unsigned short bdwSimulationIds[6] = {
        IBDW_GT0_DESK_DEVICE_F0_ID,
        IBDW_GT1_DESK_DEVICE_F0_ID,
        IBDW_GT2_DESK_DEVICE_F0_ID,
        IBDW_GT3_DESK_DEVICE_F0_ID,
        IBDW_GT4_DESK_DEVICE_F0_ID,
        0, // default, non-simulation
    };
    OCLRT::MockDevice *mockDevice = nullptr;

    for (auto id : bdwSimulationIds) {
        mockDevice = createWithUsDeviceId(id);
        ASSERT_NE(mockDevice, nullptr);

        if (id == 0)
            EXPECT_FALSE(mockDevice->isSimulation());
        else
            EXPECT_TRUE(mockDevice->isSimulation());
        delete mockDevice;
    }
}

BDWTEST_F(BdwUsDeviceIdTest, GivenBDWWhenCheckftr64KBpagesThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftr64KBpages);
}
