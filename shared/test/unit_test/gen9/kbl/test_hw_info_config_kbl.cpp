/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/kbl/device_ids_configs_kbl.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

using KblProductHelper = ProductHelperTest;

KBLTEST_F(KblProductHelper, whenGettingDefaultRevisionIdThen9IsReturned) {
    EXPECT_EQ(9u, productHelper->getDefaultRevisionId());
}

KBLTEST_F(KblProductHelper, GivenIncorrectDataWhenConfiguringHwInfoThenErrorIsReturned) {

    GT_SYSTEM_INFO &gtSystemInfo = pInHwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&pInHwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

KBLTEST_F(KblProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Kbl, productHelper->getAubStreamProductFamily());
}

KBLTEST_F(KblProductHelper, givenBoolWhenCallKblHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100030008,
        0x200030008,
        0x300030008,
        0x100020006,
        0x100030006};
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;
    PLATFORM &platform = hwInfo.platform;

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            platform.usRevId = 9;
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.flags.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waLosslessCompressionSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
            EXPECT_EQ(false, workaroundTable.flags.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(false, workaroundTable.flags.waEncryptedEdramOnlyPartials);

            platform.usRevId = 1;
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, workaroundTable.flags.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(true, workaroundTable.flags.waEncryptedEdramOnlyPartials);
        }
    }
}

KBLTEST_F(KblProductHelper, givenProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {

    for (const auto &deviceId : amlDeviceIds) {
        pInHwInfo.platform.usDeviceID = deviceId;
        EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::AML);
    }
    for (const auto &deviceId : kblDeviceIds) {
        pInHwInfo.platform.usDeviceID = deviceId;
        EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::KBL);
    }

    pInHwInfo.platform.usDeviceID = 0u;
    EXPECT_EQ(productHelper->getProductConfigFromHwInfo(pInHwInfo), AOT::UNKNOWN_ISA);
}

KBLTEST_F(KblProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

KBLTEST_F(KblProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_TRUE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_FALSE(productHelper->getScmPropertyCoherencyRequiredSupport());
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
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(productHelper->getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}
