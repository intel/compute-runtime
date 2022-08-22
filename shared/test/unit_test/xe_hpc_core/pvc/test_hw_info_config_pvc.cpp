/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"

using namespace NEO;

using PVCHwInfoConfig = ::testing::Test;

PVCTEST_F(PVCHwInfoConfig, givenPVCRevId3AndAboveWhenGettingThreadEuRatioForScratchThen16IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 3;
    EXPECT_EQ(16u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));

    hwInfo.platform.usRevId = 4;
    EXPECT_EQ(16u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PVCHwInfoConfig, givenPVCRevId0WhenGettingThreadEuRatioForScratchThen8IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usRevId = 0;
    EXPECT_EQ(8u, hwInfoConfig.getThreadEuRatioForScratch(hwInfo));
}

PVCTEST_F(PVCHwInfoConfig, givenPVCWithDifferentSteppingsThenImplicitScalingIsEnabledForBAndHigher) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    auto hwInfo = *defaultHwInfo;

    for (uint32_t stepping = 0; stepping < 0x10; stepping++) {
        auto hwRevIdFromStepping = hwInfoConfig.getHwRevIdFromStepping(stepping, hwInfo);
        if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
            hwInfo.platform.usRevId = hwRevIdFromStepping;
            const bool shouldSupportImplicitScaling = hwRevIdFromStepping >= REVISION_B;
            EXPECT_EQ(shouldSupportImplicitScaling, hwInfoConfig.isImplicitScalingSupported(hwInfo)) << "hwRevId: " << hwRevIdFromStepping;
        }
    }
}

PVCTEST_F(PVCHwInfoConfig, givenPvcHwInfoWhenIsIpSamplingSupportedThenCorrectResultIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;

    for (const auto &deviceId : pvcXlDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }

    for (const auto &deviceId : pvcXtDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }
}

PVCTEST_F(PVCHwInfoConfig, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

PVCTEST_F(PVCHwInfoConfig, givenPVCHwInfoConfigWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_FALSE(hwInfoConfig.isPrefetcherDisablingInDirectSubmissionRequired());
}

PVCTEST_F(PVCHwInfoConfig, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_TRUE(hwInfoConfig.getScmPropertyThreadArbitrationSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencySupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyLargeGrfSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionSupport());

    EXPECT_FALSE(hwInfoConfig.getSbaPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getSbaPropertyStatelessMocsSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());
}
