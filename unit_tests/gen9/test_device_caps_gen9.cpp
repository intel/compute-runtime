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

typedef Test<DeviceFixture> Gen9DeviceCaps;

GEN9TEST_F(Gen9DeviceCaps, skuSpecificCaps) {
    const auto &caps = pDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    if (pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64) {
        EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_NE(0u, caps.doubleFpConfig);
    } else {
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_EQ(0u, caps.doubleFpConfig);
    }
}

GEN9TEST_F(Gen9DeviceCaps, allSkusSupportCorrectlyRoundedDivideSqrt) {
    const auto &caps = pDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN9TEST_F(Gen9DeviceCaps, defaultPreemptionMode) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN9TEST_F(Gen9DeviceCaps, whitelistedRegisters) {
    EXPECT_TRUE(pDevice->getWhitelistedRegisters().csChicken1_0x2580);
    EXPECT_FALSE(pDevice->getWhitelistedRegisters().chicken0hdc_0xE5F0);
}

GEN9TEST_F(Gen9DeviceCaps, compression) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrCompression);
}

GEN9TEST_F(Gen9DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.pPlatform->eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.pSysInfo->MaxSubSlicesSupported * hwInfo.pSysInfo->MaxEuPerSubSlice *
                             hwInfo.pSysInfo->ThreadCount / hwInfo.pSysInfo->EUCount;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}
