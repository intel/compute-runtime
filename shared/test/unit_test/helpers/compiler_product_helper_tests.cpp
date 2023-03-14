/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CompilerProductHelperFixture = Test<DeviceFixture>;

HWTEST_F(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, false);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_FALSE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, true);
    EXPECT_TRUE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
}

TEST(CompilerProductHelperTest, WhenCompilerProductHelperCreateIsCalledWithUnknownProductThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, CompilerProductHelper::create(IGFX_UNKNOWN));
}

using IsBeforeXeHpc = IsBeforeGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenProductBeforeXeHpcWhenIsForceToStatelessRequiredThenFalseIsReturned, IsBeforeXeHpc) {
    auto &compilerProductHelper = getHelper<CompilerProductHelper>();
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

using IsAtLeastXeHpc = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterWhenIsForceToStatelessRequiredThenCorrectResultIsReturned, IsAtLeastXeHpc) {
    DebugManagerStateRestore restorer;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(false);
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    DebugManager.flags.DisableForceToStateless.set(true);
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

HWTEST2_F(CompilerProductHelperFixture, GivenGen11AndLaterThenSubgroupLocalBlockIoIsSupported, IsAtLeastGen11) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupLocalBlockIoSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenGen9OrBeforeThenSubgroupLocalBlockIoIsNotSupported, IsAtMostGen9) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupLocalBlockIoSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenDotAccumulateIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenDotAccumulateIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenCreateBufferWithPropertiesIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenCreateBufferWithPropertiesIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupNamedBarrierIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupNamedBarrierIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupExtendedBlockReadIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupExtendedBlockReadIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenBFloat16ConversionIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isBFloat16ConversionSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenMatrixMultiplyAccumulateIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeFamilyThenSplitMatrixMultiplyAccumulateIsSupported, IsWithinXeGfxFamily) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, GivenNotXeFamilyThenSplitMatrixMultiplyAccumulateIsNotSupported, IsNotWithinXeGfxFamily) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenBFloat16ConversionIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isBFloat16ConversionSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenMatrixMultiplyAccumulateIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(pDevice->getHardwareInfo()));
}

HWTEST2_F(CompilerProductHelperFixture, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostDg2) {
    auto hwInfo = *defaultHwInfo;
    auto &productHelper = getHelper<ProductHelper>();
    auto productConfig = productHelper.getProductConfigFromHwInfo(*defaultHwInfo);
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.revision);
    EXPECT_EQ(hwInfo.ipVersion.value, aotConfig.value);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtMostXeHPWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtMostXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(false), nullptr);
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(true), nullptr);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption, IsAtLeastXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteBackPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreAndDebugFlagSetForceAllResourcesUncachedWhenGetCachingPolicyOptionsThenReturnUncachedPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    DebugManager.flags.ForceAllResourcesUncached.set(true);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=1 -cl-load-cache-default=1";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenCachePolicyWithoutCorrespondingBuildOptionWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(5);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(false));
    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(true));
}
