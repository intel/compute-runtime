/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "platforms.h"

using namespace NEO;

using LkfProductHelper = ProductHelperTest;

LKFTEST_F(LkfProductHelper, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {

    GT_SYSTEM_INFO &gtSystemInfo = pInHwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&pInHwInfo, false, config, nullptr));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

LKFTEST_F(LkfProductHelper, givenProductHelperStringThenAfterSetupResultingVmeIsDisabled) {

    uint64_t config = 0x100080008;
    hardwareInfoSetup[productFamily](&pInHwInfo, false, config, nullptr);
    EXPECT_FALSE(pInHwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pInHwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(pInHwInfo.capabilityTable.supportsVme);
}

LKFTEST_F(LkfProductHelper, givenBoolWhenCallLkfHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};

    GT_SYSTEM_INFO &gtSystemInfo = pInHwInfo.gtSystemInfo;
    FeatureTable &featureTable = pInHwInfo.featureTable;
    WorkaroundTable &workaroundTable = pInHwInfo.workaroundTable;

    uint64_t config = 0x100080008;

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&pInHwInfo, setParamBool, config, nullptr);

        EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrDisplayYTiling);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
    }
}

LKFTEST_F(LkfProductHelper, givenCompilerProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {
    EXPECT_EQ(compilerProductHelper->getHwIpVersion(pInHwInfo), AOT::LKF);
}

LKFTEST_F(LkfProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

LKFTEST_F(LkfProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_TRUE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport());
    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}
