/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

struct XeLpgProductHelperTests : public ::Test<DeviceFixture> {
    void SetUp() override {
        ::Test<DeviceFixture>::SetUp();
        productHelper = &pDevice->getProductHelper();
        gfxCoreHelper = &pDevice->getGfxCoreHelper();
        ASSERT_NE(nullptr, pDevice->getReleaseHelper());
        releaseHelper = pDevice->getReleaseHelper();
    }
    ReleaseHelper *releaseHelper = nullptr;
    const ProductHelper *productHelper = nullptr;
    const GfxCoreHelper *gfxCoreHelper = nullptr;
};

using XeLpgHwInfoTests = ::testing::Test;
HWTEST2_F(XeLpgHwInfoTests, whenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, compilerProductHelper->getHwInfoConfig(hwInfo), false, *compilerProductHelper);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

HWTEST2_F(XeLpgHwInfoTests, whenCheckDirectSubmissionEnginesThenProperValuesAreSetToTrue, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &directSubmissionEngines = hwInfo.capabilityTable.directSubmissionEngines;

    for (uint32_t i = 0; i < aub_stream::NUM_ENGINES; i++) {
        switch (i) {
        case aub_stream::ENGINE_CCS:
            EXPECT_TRUE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_TRUE(directSubmissionEngines.data[i].useRootDevice);
            EXPECT_FALSE(directSubmissionEngines.data[i].useInternal);
            EXPECT_FALSE(directSubmissionEngines.data[i].useLowPriority);
            break;
        default:
            EXPECT_FALSE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_FALSE(directSubmissionEngines.data[i].useRootDevice);
            EXPECT_FALSE(directSubmissionEngines.data[i].useInternal);
            EXPECT_FALSE(directSubmissionEngines.data[i].useLowPriority);
        }
    }
}

HWTEST2_F(XeLpgHwInfoTests, WhenSetupHardwareInfoThenCorrectValuesOfCCSAndMultiTileInfoAreSet, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    hardwareInfoSetup[hwInfo.platform.eProductFamily](&hwInfo, compilerProductHelper->getHwInfoConfig(hwInfo), false, *compilerProductHelper);

    EXPECT_FALSE(gtSystemInfo.MultiTileArchInfo.IsValid);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_TRUE(1u == gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_TRUE(0b1u == gtSystemInfo.CCSInfo.Instances.CCSEnableMask);
}

HWTEST2_F(XeLpgHwInfoTests, givenBoolWhenCallHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto setParamBool : ::testing::Bool()) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, compilerProductHelper->getHwInfoConfig(hwInfo), *compilerProductHelper);

        EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrLinearCCS);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSNode);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSRing);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTile64Optimization);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.flags.waUntypedBufferCompression);
        EXPECT_FALSE(featureTable.flags.ftrTileY);
        EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
        EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
        EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
        EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    }
}

HWTEST2_F(XeLpgHwInfoTests, whenUsingCorrectConfigValueThenCorrectHwInfoIsReturned, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0x200040010;
    gtSystemInfo = {0};
    setHwInfoValuesFromConfig(config, hwInfo);
    hardwareInfoSetup[productFamily](&hwInfo, false, config, *compilerProductHelper);
    EXPECT_EQ(2u, gtSystemInfo.SliceCount);
    EXPECT_EQ(8u, gtSystemInfo.DualSubSliceCount);
}

HWTEST2_F(XeLpgHwInfoTests, GivenEmptyHwInfoForUnitTestsWhenSetupHardwareInfoIsCalledThenNonZeroValuesAreSet, IsXeLpg) {
    HardwareInfo hwInfoToSet = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfoToSet.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfoToSet.gtSystemInfo;
    gtSystemInfo = {};

    hardwareInfoSetup[productFamily](&hwInfoToSet, false, compilerProductHelper->getHwInfoConfig(hwInfoToSet), *compilerProductHelper);

    EXPECT_GT_VAL(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.EUCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxSubSlicesSupported, 0u);

    EXPECT_GT_VAL(gtSystemInfo.L3BankCount, 0u);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_GT_VAL(gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 0u);

    EXPECT_NE_VAL(hwInfoToSet.featureTable.ftrBcsInfo, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);

    for (uint32_t i = 0; i < gtSystemInfo.SliceCount; i++) {
        EXPECT_TRUE(gtSystemInfo.SliceInfo[i].Enabled);
    }
}

HWTEST2_F(XeLpgProductHelperTests, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned, IsXeLpg) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(2u, productHelper->overridePatIndex(isUncached, patIndex));

    isUncached = false;
    EXPECT_EQ(patIndex, productHelper->overridePatIndex(isUncached, patIndex));
}

HWTEST2_F(XeLpgProductHelperTests, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned, IsXeLpg) {
    EXPECT_EQ(aub_stream::ProductFamily::Mtl, productHelper->getAubStreamProductFamily());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckDirectSubmissionConstantCacheInvalidationNeededThenTrueIsReturned, IsXeLpg) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDirectSubmissionConstantCacheInvalidationNeeded(hwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFalse, IsXeLpg) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue, IsXeLpg) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckBlitEnqueueAllowedThenReturnFalse, IsXeLpg) {
    EXPECT_FALSE(productHelper->blitEnqueueAllowed());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckDpasSupportedThenReturnTrue, IsXeLpg) {
    EXPECT_TRUE(productHelper->isPlatformDpasSupported());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenCheckDp4aSupportedThenReturnTrue, IsXeLpg) {
    EXPECT_TRUE(productHelper->isPlatformDp4aSupported());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues, IsXeLpg) {

    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_TRUE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_FALSE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_TRUE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

HWTEST2_F(XeLpgProductHelperTests, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned, IsXeLpg) {

    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

HWTEST2_F(XeLpgProductHelperTests, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue, IsXeLpg) {
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_FALSE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo.capabilityTable.ftrRenderCompressedImages = false;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = false;
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));

    hwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    hwInfo.capabilityTable.ftrRenderCompressedImages = true;
    EXPECT_TRUE(productHelper->isPageTableManagerSupported(hwInfo));
}

HWTEST2_F(XeLpgProductHelperTests, givenHwIpVersionWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenAllowBasedOnReleaseHelper, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto isRcs = false;

    AOT::PRODUCT_CONFIG ipReleases[] = {AOT::MTL_M_A0, AOT::MTL_M_B0, AOT::MTL_P_A0, AOT::MTL_P_B0};
    for (auto &ipRelease : ipReleases) {
        hwInfo.ipVersion.value = ipRelease;
        auto releaseHelper = ReleaseHelper::create(ipRelease);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper.get());

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_EQ(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(), isBasicWARequired);
    }
}

HWTEST2_F(XeLpgProductHelperTests, givenReleaseHelperNullptrWhenCallingGetMediaFrequencyTileIndexThenReturnFalse, IsXeLpg) {
    uint32_t tileIndex = 0;
    ReleaseHelper *releaseHelper = nullptr;
    EXPECT_FALSE(productHelper->getMediaFrequencyTileIndex(releaseHelper, tileIndex));
}

HWTEST2_F(XeLpgProductHelperTests, whenCheckFp64SupportThenReturnTrue, IsXeLpg) {
    EXPECT_TRUE(pDevice->getHardwareInfo().capabilityTable.ftrSupportsFP64);
}

HWTEST2_F(XeLpgProductHelperTests, givenAllocationThenCheckResourceCompatibilityReturnsTrue, IsXeLpg) {
    auto allocation = std::make_unique<GraphicsAllocation>(0, AllocationType::BUFFER, nullptr, 0u, 0, MemoryPool::MemoryNull, 3u, 0llu);
    EXPECT_TRUE(gfxCoreHelper->checkResourceCompatibility(*allocation));
}

HWTEST2_F(XeLpgProductHelperTests, givenIsCompressionEnabledAndWaAuxTable64KGranularWhenCheckIs1MbAlignmentSupportedThenReturnCorrectValue, IsXeLpg) {
    auto hardwareInfo = *defaultHwInfo;
    auto isCompressionEnabled = true;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = true;
    EXPECT_FALSE(gfxCoreHelper->is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = false;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = true;
    EXPECT_FALSE(gfxCoreHelper->is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = false;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = false;
    EXPECT_FALSE(gfxCoreHelper->is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));

    isCompressionEnabled = true;
    hardwareInfo.workaroundTable.flags.waAuxTable64KGranular = false;
    EXPECT_TRUE(gfxCoreHelper->is1MbAlignmentSupported(hardwareInfo, isCompressionEnabled));
}

HWTEST2_F(XeLpgProductHelperTests, whenSetForceNonCoherentThenNothingChanged, IsXeLpg) {
    using FORCE_NON_COHERENT = typename FamilyType::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    auto stateComputeMode = FamilyType::cmdInitStateComputeMode;
    auto properties = StateComputeModeProperties{};

    properties.isCoherencyRequired.set(true);
    productHelper->setForceNonCoherent(&stateComputeMode, properties);
    EXPECT_EQ(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED, stateComputeMode.getForceNonCoherent());
    EXPECT_EQ(0u, stateComputeMode.getMaskBits());
}

HWTEST2_F(XeLpgProductHelperTests, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet, IsXeLpg) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::MTL_M_A0);
}

HWTEST2_F(XeLpgProductHelperTests, whenCheckDirectSubmissionSupportedThenValueFromReleaseHelperIsReturned, IsXeLpg) {
    EXPECT_EQ(productHelper->isDirectSubmissionSupported(releaseHelper), releaseHelper->isDirectSubmissionSupported());
}
