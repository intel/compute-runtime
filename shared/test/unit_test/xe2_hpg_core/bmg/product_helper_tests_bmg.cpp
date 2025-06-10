/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_bmg.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "neo_aot_platforms.h"

using namespace NEO;

using BmgProductHelper = ProductHelperTest;

BMGTEST_F(BmgProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Bmg, productHelper->getAubStreamProductFamily());
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

BMGTEST_F(BmgProductHelper, givenBmgProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenCheckIsCopyBufferRectSplitSupportedThenReturnsTrue) {
    EXPECT_TRUE(productHelper->isCopyBufferRectSplitSupported());
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_TRUE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_TRUE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_TRUE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_FALSE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_FALSE(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

BMGTEST_F(BmgProductHelper, whenCheckPreferredAllocationMethodThenAllocateByKmdIsReturnedExceptTagBufferAndTimestampPacketTagBuffer) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

BMGTEST_F(BmgProductHelper, WhenFillingScmPropertiesSupportThenExpectUseCorrectExtraGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};

    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(true, scmPropertiesSupport.allocationForScratchAndMidthreadPreemption);
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(*defaultHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

BMGTEST_F(BmgProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersonThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::BMG_G21_A0);
}

HWTEST_EXCLUDE_PRODUCT(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned, IGFX_BMG);
BMGTEST_F(BmgProductHelper, givenCompilerProductHelperWhenGetMidThreadPreemptionSupportThenCorrectValueIsSet) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrWalkerMTP = false;
    EXPECT_FALSE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
    hwInfo.featureTable.flags.ftrWalkerMTP = true;
    EXPECT_TRUE(compilerProductHelper->isMidThreadPreemptionSupported(hwInfo));
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenAdjustNumberOfCcsThenOverrideToSingleCcs) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 223;
    productHelper->adjustNumberOfCcs(hwInfo);
    EXPECT_EQ(hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 1u);
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

BMGTEST_F(BmgProductHelper, whenAdjustScratchSizeThenSizeIsDoubled) {
    constexpr size_t initialScratchSize = 0x1234u;
    size_t scratchSize = initialScratchSize;
    productHelper->adjustScratchSize(scratchSize);
    EXPECT_EQ(initialScratchSize * 2, scratchSize);
}

BMGTEST_F(BmgProductHelper, givenProductHelperWhenCallDeferMOCSToPatOnWSLThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->deferMOCSToPatIndex(true));
}
