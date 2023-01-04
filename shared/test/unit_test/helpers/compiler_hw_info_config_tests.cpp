/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CompilerProductHelperFixture = Test<DeviceFixture>;

HWTEST_F(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, false);
    auto compilerProductHelper = CompilerProductHelper::get(hwInfo.platform.eProductFamily);
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, true);
    EXPECT_TRUE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}

HWTEST_F(CompilerProductHelperFixture, WhenAdjustHwInfoForIgcIsCalledThenHwInfoNotChanged) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto adjustedHwInfo = hwInfo;
    auto compilerProductHelper = CompilerProductHelper::get(hwInfo.platform.eProductFamily);

    compilerProductHelper->adjustHwInfoForIgc(adjustedHwInfo);

    EXPECT_EQ(hwInfo.platform.eProductFamily, adjustedHwInfo.platform.eProductFamily);
}

using IsBeforeXeHpc = IsBeforeGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenProductBeforeXeHpcWhenIsForceToStatelessRequiredThenFalseIsReturned, IsBeforeXeHpc) {
    auto &compilerProductHelper = *CompilerProductHelper::get(productFamily);
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

using IsAtLeastXeHpc = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterWhenIsForceToStatelessRequiredThenCorrectResultIsReturned, IsAtLeastXeHpc) {
    DebugManagerStateRestore restorer;
    auto &compilerProductHelper = *CompilerProductHelper::get(productFamily);
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(false);
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(true);
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

HWTEST2_F(CompilerProductHelperFixture, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostDg2) {
    auto hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    auto productConfig = productHelper.getProductConfigFromHwInfo(*defaultHwInfo);
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    CompilerProductHelper::get(hwInfo.platform.eProductFamily)->setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.revision);
    EXPECT_EQ(hwInfo.ipVersion.value, aotConfig.value);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtMostXeHPWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtMostXeHpCore) {
    auto compilerProductHelper = CompilerProductHelper::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(compilerProductHelper->getCachingPolicyOptions(false), nullptr);
    EXPECT_EQ(compilerProductHelper->getCachingPolicyOptions(true), nullptr);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption, IsAtLeastXeHpgCore) {
    auto compilerProductHelper = CompilerProductHelper::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteBackPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    auto compilerProductHelper = CompilerProductHelper::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreAndDebugFlagSetForceAllResourcesUncachedWhenGetCachingPolicyOptionsThenReturnUncachedPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    DebugManager.flags.ForceAllResourcesUncached.set(true);

    auto compilerProductHelper = CompilerProductHelper::get(defaultHwInfo->platform.eProductFamily);
    const char *expectedStr = "-cl-store-cache-default=1 -cl-load-cache-default=1";
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper->getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenCachePolicyWithoutCorrespondingBuildOptionWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(5);

    auto compilerProductHelper = CompilerProductHelper::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_EQ(nullptr, compilerProductHelper->getCachingPolicyOptions(false));
    EXPECT_EQ(nullptr, compilerProductHelper->getCachingPolicyOptions(true));
}
