/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "hw_cmds.h"
#include "neo_aot_platforms.h"

using namespace NEO;

using Dg1HwInfo = ::testing::Test;

DG1TEST_F(Dg1HwInfo, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, config, nullptr));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

DG1TEST_F(Dg1HwInfo, givenBoolWhenCallDg1HardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t config = 0x100060010;
    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config, nullptr);

        EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrLocalMemory);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_FALSE(featureTable.flags.ftrHeaplessMode);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
    }
}

using Dg1ProductHelper = ProductHelperTest;

DG1TEST_F(Dg1ProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Dg1, productHelper->getAubStreamProductFamily());
}

DG1TEST_F(Dg1ProductHelper, givenA0SteppingAndDg1PlatformWhenAskingIfWAIsRequiredThenReturnTrue) {

    std::array<std::pair<uint32_t, bool>, 2> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);

        productHelper->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper->pipeControlWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper->imagePitchAlignmentWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper->isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

DG1TEST_F(Dg1ProductHelper, givenProductHelperWhenAskedIfStorageInfoAdjustmentIsRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isStorageInfoAdjustmentRequired());
}

DG1TEST_F(Dg1ProductHelper, givenProductHelperWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->is3DPipelineSelectWARequired());
}

DG1TEST_F(Dg1ProductHelper, givenDg1WhenObtainingBlitterPreferenceThenReturnTrue) {
    const auto &hardwareInfo = DG1::hwInfo;
    EXPECT_TRUE(productHelper->obtainBlitterPreference(hardwareInfo));
}

DG1TEST_F(Dg1ProductHelper, whenConfigureHwInfoThenBlitterSupportIsEnabled) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.capabilityTable.blitterOperationsSupported = false;
    productHelper->configureHardwareCustom(&hardwareInfo, nullptr);

    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

DG1TEST_F(Dg1ProductHelper, givenDg1WhenObtainingFullBlitterSupportThenReturnFalse) {
    const auto &hardwareInfo = DG1::hwInfo;

    EXPECT_FALSE(productHelper->isBlitterFullySupported(hardwareInfo));
}

DG1TEST_F(Dg1ProductHelper, whenOverrideGfxPartitionLayoutForWslThenReturnTrue) {
    EXPECT_TRUE(productHelper->overrideGfxPartitionLayoutForWsl());
}

DG1TEST_F(Dg1ProductHelper, givenCompilerProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {
    EXPECT_EQ(compilerProductHelper->getHwIpVersion(*defaultHwInfo), AOT::DG1);
}

DG1TEST_F(Dg1ProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

DG1TEST_F(Dg1ProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

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

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}
