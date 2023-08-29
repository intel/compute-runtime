/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds_mtl.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"
#include "shared/test/unit_test/xe_hpg_core/os_agnostic_product_helper_xe_lpg_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

using MtlProductHelper = ProductHelperTest;

MTLTEST_F(MtlProductHelper, givenMtlConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    MtlHwConfig::setupHardwareInfoBase(&hwInfo, false, *compilerProductHelper);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}

MTLTEST_F(MtlProductHelper, givenMtlConfigWhenSetupHardwareInfoThenGtSystemInfoHasNonZeroValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    MtlHwConfig::setupHardwareInfo(&hwInfo, false, *compilerProductHelper);

    EXPECT_GT(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT(gtSystemInfo.EUCount, 0u);
    EXPECT_GT(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT_VAL(gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_GT(gtSystemInfo.L3BankCount, 0u);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_GT(gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 0u);
}

MTLTEST_F(MtlProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Mtl, productHelper->getAubStreamProductFamily());
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(pInHwInfo));
}

MTLTEST_F(MtlProductHelper, givenMtlProductHelperWhenCheckDirectSubmissionConstantCacheInvalidationNeededThenTrueIsReturned) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDirectSubmissionConstantCacheInvalidationNeeded(hwInfo));
}

MTLTEST_F(MtlProductHelper, givenMtlProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

MTLTEST_F(MtlProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

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

MTLTEST_F(MtlProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(hwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, hwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

MTLTEST_F(MtlProductHelper, givenCompressionFtrEnabledWhenAskingForPageTableManagerThenReturnCorrectValue) {
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

MTLTEST_F(MtlProductHelper, givenHwIpVersionWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenAllowBasedOnReleaseHelper) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto isRcs = false;

    AOT::PRODUCT_CONFIG ipReleases[] = {AOT::MTL_M_A0, AOT::MTL_M_B0, AOT::MTL_P_A0, AOT::MTL_P_B0};
    for (auto &ipRelease : ipReleases) {
        hwInfo.ipVersion.value = ipRelease;
        refreshReleaseHelper(&hwInfo);

        const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);

        EXPECT_FALSE(isExtendedWARequired);
        EXPECT_EQ(releaseHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(), isBasicWARequired);
    }
}

MTLTEST_F(MtlProductHelper, givenMtlAndReleaseHelperNullptrWhenCallingGetMediaFrequencyTileIndexThenReturnFalse) {
    uint32_t tileIndex = 0;
    ReleaseHelper *releaseHelper = nullptr;
    EXPECT_FALSE(productHelper->getMediaFrequencyTileIndex(releaseHelper, tileIndex));
}

MTLTEST_F(MtlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::MTL_M_A0);
}

using ProductHelperTestMtl = Test<DeviceFixture>;

MTLTEST_F(ProductHelperTestMtl, givenMtlWhenCheckIsCachingOnCpuAvailableThenAlwaysFalse) {
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_FALSE(productHelper.isCachingOnCpuAvailable());
}

MTLTEST_F(ProductHelperTestMtl, givenMtlWhenCheckPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    auto preferredAllocationMethod = productHelper.getPreferredAllocationMethod();
    EXPECT_TRUE(preferredAllocationMethod.has_value());
    EXPECT_EQ(GfxMemoryAllocationMethod::AllocateByKmd, preferredAllocationMethod.value());
}

MTLTEST_F(ProductHelperTestMtl, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned) {
    const auto &productHelper = getHelper<ProductHelper>();
    XeLpgTests::testOverridePatIndex(productHelper);
}