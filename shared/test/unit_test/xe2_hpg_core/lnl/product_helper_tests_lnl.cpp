/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "neo_aot_platforms.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}

using namespace NEO;

using LnlProductHelper = ProductHelperTest;

LNLTEST_F(LnlProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Lnl, productHelper->getAubStreamProductFamily());
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {

    EXPECT_TRUE(productHelper->isEvictionIfNecessaryFlagSupported());
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

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

    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

LNLTEST_F(LnlProductHelper, WhenFillingScmPropertiesSupportThenExpectUseCorrectExtraGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};

    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(true, scmPropertiesSupport.allocationForScratchAndMidthreadPreemption);
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(*defaultHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

LNLTEST_F(LnlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::LNL_B0);
}

LNLTEST_F(LnlProductHelper, whenCheckPreferredAllocationMethodThenAllocateByKmdIsReturnedExceptTagBufferAndTimestampPacketTagBuffer) {
    DebugManagerStateRestore restorer;
    debugManager.flags.AllowDcFlush.set(1);
    for (auto i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
    debugManager.flags.AllowDcFlush.set(0);
    for (auto i = 0; i < static_cast<int>(AllocationType::count); ++i) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        if (allocationType == AllocationType::tagBuffer ||
            allocationType == AllocationType::timestampPacketTagBuffer ||
            allocationType == AllocationType::hostFunction) {
            EXPECT_TRUE(preferredAllocationMethod.has_value());
            EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
        }
    }
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCallIsCachingOnCpuAvailableThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->isCachingOnCpuAvailable());
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenIsInitBuiltinAsyncSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isInitBuiltinAsyncSupported(*defaultHwInfo));
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckoverrideAllocationCpuCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCpuCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCpuCacheable(allocationData));
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckBlitEnqueuePreferredThenReturnCorrectValue) {
    EXPECT_TRUE(productHelper->blitEnqueuePreferred(true));
    EXPECT_FALSE(productHelper->blitEnqueuePreferred(false));
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckingIsDeviceUsmAllocationReuseSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isDeviceUsmAllocationReuseSupported());
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckingIsHostDeviceUsmPoolAllocatorSupportedThenCorrectValueIsReturned) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmPoolAllocatorSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmPoolAllocatorSupported());
    }
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isDirectSubmissionSupported(releaseHelper));
}

LNLTEST_F(LnlProductHelper, givenProductHelperWhenIsMisalignedUserPtr2WayCoherentThenReturnTrue) {
    EXPECT_TRUE(productHelper->isMisalignedUserPtr2WayCoherent());
}