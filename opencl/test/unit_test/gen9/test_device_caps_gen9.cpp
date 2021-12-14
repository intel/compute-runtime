/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using namespace NEO;

typedef Test<ClDeviceFixture> Gen9DeviceCaps;

GEN9TEST_F(Gen9DeviceCaps, WhenCheckingExtensionStringThenFp64CorrectlyReported) {
    const auto &caps = pClDevice->getDeviceInfo();
    std::string extensionString = caps.deviceExtensions;
    if (pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64) {
        EXPECT_NE(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_NE(0u, caps.doubleFpConfig);
    } else {
        EXPECT_EQ(std::string::npos, extensionString.find(std::string("cl_khr_fp64")));
        EXPECT_EQ(0u, caps.doubleFpConfig);
    }
}

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckExtensionsThenDeviceProperlyReportsClKhrSubgroupsExtension) {
    const auto &caps = pClDevice->getDeviceInfo();
    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_THAT(caps.deviceExtensions, testing::HasSubstr(std::string("cl_khr_subgroups")));
    } else {
        EXPECT_THAT(caps.deviceExtensions, ::testing::Not(testing::HasSubstr(std::string("cl_khr_subgroups"))));
    }
}

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckingCapsThenDeviceDoesProperlyReportsIndependentForwardProgress) {
    const auto &caps = pClDevice->getDeviceInfo();

    if (pClDevice->areOcl21FeaturesEnabled()) {
        EXPECT_TRUE(caps.independentForwardProgress != 0);
    } else {
        EXPECT_FALSE(caps.independentForwardProgress != 0);
    }
}

GEN9TEST_F(Gen9DeviceCaps, WhenGettingDeviceInfoThenCorrectlyRoundedDivideSqrtIsEnabled) {
    const auto &caps = pClDevice->getDeviceInfo();
    EXPECT_NE(0u, caps.singleFpConfig & CL_FP_CORRECTLY_ROUNDED_DIVIDE_SQRT);
}

GEN9TEST_F(Gen9DeviceCaps, GivenDefaultWhenCheckingPreemptionModeThenMidThreadIsSupported) {
    EXPECT_EQ(PreemptionMode::MidThread, pDevice->getHardwareInfo().capabilityTable.defaultPreemptionMode);
}

GEN9TEST_F(Gen9DeviceCaps, WhenCheckingCompressionThenItIsDisabled) {
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

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckingImageSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsImages);
}

GEN9TEST_F(Gen9DeviceCaps, givenGen9WhenCheckingMediaBlockSupportThenReturnTrue) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.supportsMediaBlock);
}
