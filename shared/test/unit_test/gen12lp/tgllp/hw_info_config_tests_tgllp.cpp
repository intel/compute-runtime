/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTestTgllp = ::testing::Test;

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_FALSE(hwInfoConfig.getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(hwInfoConfig.getSbaPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getSbaPropertyStatelessMocsSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(hwInfoConfig.getPipelineSelectPropertySystolicModeSupport());
}
