/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

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

    for (auto &deviceId : PVC_XL_IDS) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_FALSE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }

    for (auto &deviceId : PVC_XT_IDS) {
        hwInfo.platform.usDeviceID = deviceId;
        EXPECT_TRUE(hwInfoConfig.isIpSamplingSupported(hwInfo));
    }
}
