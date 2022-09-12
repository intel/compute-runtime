/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/xe_hp_core/hw_cmds_xe_hp_sdv.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using HwInfoConfigTestXeHpSdv = ::testing::Test;

XEHPTEST_F(HwInfoConfigTestXeHpSdv, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

XEHPTEST_F(HwInfoConfigTestXeHpSdv, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_FALSE(hwInfoConfig.getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionModeSupport());

    EXPECT_TRUE(hwInfoConfig.getSbaPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getSbaPropertyStatelessMocsSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertySystolicModeSupport());
}
