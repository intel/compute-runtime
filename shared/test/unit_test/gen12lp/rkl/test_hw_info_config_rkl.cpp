/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "platforms.h"

using namespace NEO;

using RklHwInfo = ::testing::Test;

RKLTEST_F(RklHwInfo, givenBoolWhenCallRklHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100020010};
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);

            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        }
    }
}

RKLTEST_F(RklHwInfo, whenPlatformIsRklThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

RKLTEST_F(RklHwInfo, givenRklWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics);
}

RKLTEST_F(RklHwInfo, givenRklWhenCheckL0ThenReturnTrue) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.levelZeroSupported);
}

using RklProductHelper = HwInfoConfigTest;

RKLTEST_F(RklProductHelper, givenProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {
    EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::RKL);
}

RKLTEST_F(RklProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

RKLTEST_F(RklProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyStatelessMocsSupport());
    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

RKLTEST_F(RklProductHelper, givenA0OrBSteppingAndRklPlatformWhenAskingIfWAIsRequiredThenReturnTrue) {

    std::array<std::pair<uint32_t, bool>, 3> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, true},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);

        productHelper->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper->isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

RKLTEST_F(RklProductHelper, givenProductHelperWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->is3DPipelineSelectWARequired());
}

using CompilerHwInfoConfigHelperTestsRkl = ::testing::Test;
RKLTEST_F(CompilerHwInfoConfigHelperTestsRkl, givenRklWhenIsForceEmuInt32DivRemSPRequiredIsCalledThenReturnsTrue) {
    EXPECT_TRUE(CompilerHwInfoConfig::get(productFamily)->isForceEmuInt32DivRemSPRequired());
}
