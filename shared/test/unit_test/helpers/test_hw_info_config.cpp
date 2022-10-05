/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_helper.h"
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

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsSystolicModeConfigurabledThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isSystolicModeConfigurable(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGetThreadEuRatioForScratchThen8IsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(8u, hwInfoConfig.getThreadEuRatioForScratch(*defaultHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenDefaultSettingWhenIsGrfNumReportedIsCalledThenScmSupportProductValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getScmPropertyLargeGrfModeSupport(), hwInfoConfig.isGrfNumReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenForceGrfNumProgrammingWithScmFlagSetWhenIsGrfNumReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(0);
    EXPECT_FALSE(hwInfoConfig.isGrfNumReportedWithScm());

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    EXPECT_TRUE(hwInfoConfig.isGrfNumReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenDefaultSettingWhenIsThreadArbitrationPolicyReportedIsCalledThenScmSupportProductValueReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getScmPropertyThreadArbitrationPolicySupport(), hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());
}

HWTEST_F(HwInfoConfigTest, givenForceThreadArbitrationPolicyProgrammingWithScmFlagSetWhenIsThreadArbitrationPolicyReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(0);
    EXPECT_FALSE(hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);
    EXPECT_TRUE(hwInfoConfig.isThreadArbitrationPolicyReportedWithScm());
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenIsImplicitScalingSupportedThenExpectFalse, IsNotXeHpOrXeHpcCore) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isImplicitScalingSupported(*defaultHwInfo));
}

HWTEST2_F(HwInfoConfigTest, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostDg2) {
    auto hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    auto productConfig = hwInfoConfig.getProductConfigFromHwInfo(*defaultHwInfo);
    AheadOfTimeConfig aotConfig = {0};
    aotConfig.ProductConfig = productConfig;
    CompilerHwInfoConfig::get(hwInfo.platform.eProductFamily)->setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.ProductConfigID.Revision);
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsAdjustWalkOrderAvailableCallThenFalseReturn) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdjustWalkOrderAvailable(*defaultHwInfo));
}

HWTEST2_F(HwInfoConfigTest, givenAtMostXeHPWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtMostXeHpCore) {
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(compilerHwInfoConfig->getCachingPolicyOptions(false), nullptr);
    EXPECT_EQ(compilerHwInfoConfig->getCachingPolicyOptions(true), nullptr);
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption, IsAtLeastXeHpgCore) {
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteBackPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenAtLeastXeHpgCoreAndDebugFlagSetForceAllResourcesUncachedWhenGetCachingPolicyOptionsThenReturnUncachedPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    DebugManager.flags.ForceAllResourcesUncached.set(true);

    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=1 -cl-load-cache-default=1";
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerHwInfoConfig->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(HwInfoConfigTest, givenCachePolicyWithoutCorrespondingBuildOptionWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(5);

    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(nullptr, compilerHwInfoConfig->getCachingPolicyOptions(false));
    EXPECT_EQ(nullptr, compilerHwInfoConfig->getCachingPolicyOptions(true));
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigAndDebugFlagWhenGetL1CachePolicyThenReturnCorrectPolicy, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    auto hwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, hwInfoConfig->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, hwInfoConfig->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, hwInfoConfig->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, hwInfoConfig->getL1CachePolicy(true));

    DebugManager.flags.ForceAllResourcesUncached.set(true);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, hwInfoConfig->getL1CachePolicy(true));
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenGetL1CachePolicyThenReturnWriteByPass, IsAtLeastXeHpgCore) {
    auto hwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, hwInfoConfig->getL1CachePolicy(true));
}

HWTEST2_F(HwInfoConfigTest, givenPlatformWithUnsupportedL1CachePoliciesWhenGetL1CachePolicyThenReturnZero, IsAtMostXeHpCore) {
    auto hwInfo = *defaultHwInfo;
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_EQ(0u, hwInfoConfig->getL1CachePolicy(false));
    EXPECT_EQ(0u, hwInfoConfig->getL1CachePolicy(true));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isPrefetcherDisablingInDirectSubmissionRequired());
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenIsStatefulAddressingModeSupportedThenReturnTrue, HasStatefulSupport) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_TRUE(hwInfoConfig.isStatefulAddressingModeSupported());
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenIsPlatformQueryNotSupportedThenReturnFalse, IsAtMostDg2) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_FALSE(hwInfoConfig.isPlatformQuerySupported());
}

HWTEST2_F(HwInfoConfigTest, givenHwHelperWhenCallCopyThroughLockedPtrEnabledThenReturnFalse, IsNotXeHpcCore) {
    HwHelper &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_FALSE(hwHelper.copyThroughLockedPtrEnabled());
}

HWTEST_F(HwInfoConfigTest, givenHwHelperWhenFlagSetAndCallCopyThroughLockedPtrEnabledThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    HwHelper &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    DebugManager.flags.ExperimentalCopyThroughLock.set(0);
    EXPECT_FALSE(hwHelper.copyThroughLockedPtrEnabled());

    DebugManager.flags.ExperimentalCopyThroughLock.set(1);
    EXPECT_TRUE(hwHelper.copyThroughLockedPtrEnabled());
}

HWTEST2_F(HwInfoConfigTest, givenHwHelperWhenCallGetAmountOfAllocationsToFillThenReturnFalse, IsNotXeHpcCore) {
    HwHelper &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    EXPECT_EQ(hwHelper.getAmountOfAllocationsToFill(), 0u);
}

HWTEST_F(HwInfoConfigTest, givenHwHelperWhenFlagSetAndCallGetAmountOfAllocationsToFillThenReturnCorrectValue) {
    DebugManagerStateRestore restorer;
    HwHelper &hwHelper = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily);
    DebugManager.flags.SetAmountOfReusableAllocations.set(0);
    EXPECT_EQ(hwHelper.getAmountOfAllocationsToFill(), 0u);

    DebugManager.flags.SetAmountOfReusableAllocations.set(1);
    EXPECT_EQ(hwHelper.getAmountOfAllocationsToFill(), 1u);
}