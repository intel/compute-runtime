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

GEN9TEST_F(Gen9DeviceCaps, compression) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedBuffers);
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.ftrRenderCompressedImages);
}

GEN9TEST_F(Gen9DeviceCaps, givenHwInfoWhenRequestedComputeUnitsUsedForScratchThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    uint32_t expectedValue = hwInfo.gtSystemInfo.MaxSubSlicesSupported * hwInfo.gtSystemInfo.MaxEuPerSubSlice *
                             hwInfo.gtSystemInfo.ThreadCount / hwInfo.gtSystemInfo.EUCount;

    EXPECT_EQ(expectedValue, hwHelper.getComputeUnitsUsedForScratch(&hwInfo));
    EXPECT_EQ(expectedValue, pDevice->getDeviceInfo().computeUnitsUsedForScratch);
}

GEN9TEST_F(Gen9DeviceCaps, givenHwInfoWhenRequestedMaxFrontEndThreadsThenReturnValidValue) {
    const auto &hwInfo = pDevice->getHardwareInfo();

    EXPECT_EQ(HwHelper::getMaxThreadsForVfe(hwInfo), pDevice->getDeviceInfo().maxFrontEndThreads);
}

GEN9TEST_F(Gen9DeviceCaps, givenHwInfoWhenSlmSizeIsRequiredThenReturnCorrectValue) {
    EXPECT_EQ(64u, pDevice->getHardwareInfo().capabilityTable.slmSize);
}

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckSupportCacheFlushAfterWalkerThenFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.supportCacheFlushAfterWalker);
}

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckBlitterOperationsSupportThenReturnFalse) {
    EXPECT_FALSE(pDevice->getHardwareInfo().capabilityTable.blitterOperationsSupported);
}