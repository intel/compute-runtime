/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adln.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

using AdlnHwInfo = ::testing::Test;

ADLNTEST_F(AdlnHwInfo, givenBoolWhenCallAdlnHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t config = 0x0;
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
        EXPECT_EQ(setParamBool, workaroundTable.flags.waUntypedBufferCompression);
    }
}

ADLNTEST_F(AdlnHwInfo, whenPlatformIsAdlnThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = ADLN::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

ADLNTEST_F(AdlnHwInfo, givenAdlnWhenCheckL0ThenReturnTrue) {
    const HardwareInfo &hardwareInfo = ADLN::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.levelZeroSupported);
}

using AdlnProductHelper = ProductHelperTest;

ADLNTEST_F(AdlnProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Adln, productHelper->getAubStreamProductFamily());
}

ADLNTEST_F(AdlnProductHelper, givenProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {
    EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::ADL_N);
}

ADLNTEST_F(AdlnProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

ADLNTEST_F(AdlnProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

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

ADLNTEST_F(AdlnProductHelper, givenAdlnPlatformWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto hwInfo = *defaultHwInfo;

    EXPECT_TRUE(productHelper->pipeControlWARequired(hwInfo));
}