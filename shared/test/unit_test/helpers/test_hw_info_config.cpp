/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "platforms.h"

using namespace NEO;

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsAdjustProgrammableIdPreferredSlmSizeRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdjustProgrammableIdPreferredSlmSizeRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInCfeStateRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsComputeDispatchAllWalkerEnableInComputeWalkerRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isComputeDispatchAllWalkerEnableInComputeWalkerRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsGlobalFenceInCommandStreamRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isGlobalFenceInCommandStreamRequired(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSpecialPipelineSelectModeChangedThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSpecialPipelineSelectModeChanged(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSystolicModeConfigurable(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGetThreadEuRatioForScratchThen8IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(8u, hwInfoConfig.getThreadEuRatioForScratch(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, whenIsGrfNumReportedWithScmIsQueriedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isGrfNumReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenForceGrfNumProgrammingWithScmFlagSetWhenIsGrfNumReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(0);
    EXPECT_FALSE(hwInfoConfig.isGrfNumReportedWithScm());

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    EXPECT_TRUE(hwInfoConfig.isGrfNumReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, whenIsThreadArbitrationPolicyReportedWithScmIsQueriedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenForceThreadArbitrationPolicyProgrammingWithScmFlagSetWhenIsThreadArbitrationPolicyReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(0);
    EXPECT_FALSE(hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);
    EXPECT_TRUE(hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenIsImplicitScalingSupportedThenExpectFalse, isNotXeHpOrXeHpcCore) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isImplicitScalingSupported(*defaultHwInfo));
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound, IsAtMostXeHpCore) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_NE(hwInfoConfig.getProductConfigFromHwInfo(*defaultHwInfo), AOT::UNKNOWN_ISA);
}

HWTEST2_F(HwInfoConfigTest, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostXeHpCore) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto productConfig = hwInfoConfig.getProductConfigFromHwInfo(*defaultHwInfo);
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = productConfig;
    CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->setProductConfigForHwInfo(*defaultHwInfo, aotConfig);
    EXPECT_EQ(defaultHwInfo->platform.usRevId, aotConfig.ProductConfigID.Revision);
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsAdjustWalkOrderAvailableCallThenFalseReturn) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdjustWalkOrderAvailable(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenCompilerHwInfoConfigWhengetCachingPolicyOptionsThenReturnNullptr) {
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(compilerHwInfoConfig->getCachingPolicyOptions(), nullptr);
}