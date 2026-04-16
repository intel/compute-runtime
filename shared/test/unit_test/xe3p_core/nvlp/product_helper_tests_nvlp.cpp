/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "neo_aot_platforms.h"
#include "per_product_test_definitions.h"

using namespace NEO;

using NvlProductHelper = ProductHelperTest;

NVLPTEST_F(NvlProductHelper, whenGettingPreferredAllocationMethodThenAllocateByKmdIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_TRUE(preferredAllocationMethod.has_value());
        EXPECT_EQ(GfxMemoryAllocationMethod::allocateByKmd, preferredAllocationMethod.value());
    }
}

NVLPTEST_F(NvlProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Nvlp, productHelper->getAubStreamProductFamily());
}

NVLPTEST_F(NvlProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
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
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

NVLPTEST_F(NvlProductHelper, WhenFillingScmPropertiesSupportThenExpectUseCorrectExtraGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(true, scmPropertiesSupport.allocationForScratchAndMidthreadPreemption);
    EXPECT_EQ(true, scmPropertiesSupport.enableVariableRegisterSizeAllocation);
}

NVLPTEST_F(NvlProductHelper, WhenFillingScmPropertiesSupportExtraThenExpectUseCorrectExtraGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport{};
    MockExecutionEnvironment executionEnvironment{};
    const auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    productHelper->fillScmPropertiesSupportStructureExtra(scmPropertiesSupport, rootDeviceEnvironment);

    EXPECT_EQ(false, scmPropertiesSupport.enableOutOfBoundariesInTranslationException);
}

NVLPTEST_F(NvlProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(*defaultHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

NVLPTEST_F(NvlProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::NVL_P_B0);
}

NVLPTEST_F(NvlProductHelper, givenCompilerProductHelperWhenIsHeaplessModeEnabledThenCorrectValueIsSet) {
    DebugManagerStateRestore restorer;

    debugManager.flags.Enable64BitAddressing.set(-1);
    EXPECT_TRUE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(0);
    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(1);
    EXPECT_TRUE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));
}

NVLPTEST_F(NvlProductHelper, givenFtrHeaplessModeFalseWhenIsHeaplessModeEnabledThen64BitAddressingIsAlwaysFalse) {
    DebugManagerStateRestore restorer;
    VariableBackup<FeatureTableBase::Flags> ftrHeaplessModeBackup{&defaultHwInfo->featureTable.flags};
    defaultHwInfo->featureTable.flags.ftrHeaplessMode = false;

    debugManager.flags.Enable64BitAddressing.set(-1);
    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(0);
    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(1);
    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));
}

NVLPTEST_F(NvlProductHelper, givenEnableExtendedScratchSurfaceSizeFlagWhenCallIsAvailableExtendedScratchThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(1);
    EXPECT_TRUE(productHelper->isAvailableExtendedScratch());

    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    EXPECT_FALSE(productHelper->isAvailableExtendedScratch());
}

NVLPTEST_F(NvlProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

NVLPTEST_F(NvlProductHelper, givenProductHelperWhenCheckoverrideAllocationCpuCacheableThenTrueIsReturnedForCommandBuffer) {
    AllocationData allocationData{};
    allocationData.type = AllocationType::commandBuffer;
    EXPECT_TRUE(productHelper->overrideAllocationCpuCacheable(allocationData));

    allocationData.type = AllocationType::buffer;
    EXPECT_FALSE(productHelper->overrideAllocationCpuCacheable(allocationData));
}
