/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_bdw.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;
using BdwProductHelper = ProductHelperTest;

BDWTEST_F(BdwProductHelper, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {

    GT_SYSTEM_INFO &gtSystemInfo = pInHwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&pInHwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

BDWTEST_F(BdwProductHelper, givenBoolWhenCallBdwHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100030008,
        0x200030008,
        0x100020006,
        0x100030006};
    bool boolValue[]{
        true, false};

    GT_SYSTEM_INFO &gtSystemInfo = pInHwInfo.gtSystemInfo;
    FeatureTable &featureTable = pInHwInfo.featureTable;
    WorkaroundTable &workaroundTable = pInHwInfo.workaroundTable;

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&pInHwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.flags.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waUseVAlign16OnTileXYBpp816);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
        }
    }
}

BDWTEST_F(BdwProductHelper, givenProductHelperStringThenAfterSetupResultingVmeIsDisabled) {

    uint64_t config = 0x0;
    hardwareInfoSetup[productFamily](&pInHwInfo, false, config);
    EXPECT_FALSE(pInHwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(pInHwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(pInHwInfo.capabilityTable.supportsVme);
}

BDWTEST_F(BdwProductHelper, givenProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {

    EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::BDW);
}

BDWTEST_F(BdwProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

BDWTEST_F(BdwProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Bdw, productHelper->getAubStreamProductFamily());
}

BDWTEST_F(BdwProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

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
    EXPECT_FALSE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

using CompilerProductHelperHelperTestsBdw = ::testing::Test;
BDWTEST_F(CompilerProductHelperHelperTestsBdw, givenBdwWhenIsStatelessToStatefulBufferOffsetSupportedIsCalledThenReturnsTrue) {
    EXPECT_FALSE(CompilerProductHelper::get(productFamily)->isStatelessToStatefulBufferOffsetSupported());
}
