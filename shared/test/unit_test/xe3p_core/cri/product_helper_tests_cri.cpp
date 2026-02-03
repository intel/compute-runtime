/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/compiler_interface/compiler_options.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/xe3p_core/hw_cmds_cri.h"
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

using CriProductHelper = ProductHelperTest;

CRITEST_F(CriProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Cri, productHelper->getAubStreamProductFamily());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
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

CRITEST_F(CriProductHelper, WhenFillingScmPropertiesSupportThenExpectUseCorrectExtraGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};
    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(true, scmPropertiesSupport.allocationForScratchAndMidthreadPreemption);
    EXPECT_EQ(true, scmPropertiesSupport.enableVariableRegisterSizeAllocation);
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isDisableOverdispatchAvailable(*defaultHwInfo));

    FrontEndPropertiesSupport fePropertiesSupport{};
    productHelper->fillFrontEndPropertiesSupportStructure(fePropertiesSupport, *defaultHwInfo);
    EXPECT_TRUE(fePropertiesSupport.disableOverdispatch);
}

CRITEST_F(CriProductHelper, whenQueryingMaxNumSamplersThenReturnZero) {
    EXPECT_EQ(0u, productHelper->getMaxNumSamplers());
}

CRITEST_F(CriProductHelper, givenCompilerProductHelperWhenGetDefaultHwIpVersionThenCorrectValueIsSet) {
    EXPECT_EQ(compilerProductHelper->getDefaultHwIpVersion(), AOT::CRI_A0);
}

CRITEST_F(CriProductHelper, givenCompilerProductHelperWhenIsHeaplessModeEnabledThenCorrectValueIsSet) {
    DebugManagerStateRestore restorer;

    debugManager.flags.Enable64BitAddressing.set(-1);
    EXPECT_TRUE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(0);
    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));

    debugManager.flags.Enable64BitAddressing.set(1);
    EXPECT_TRUE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));
}

CRITEST_F(CriProductHelper, givenFtrHeaplessModeFalseWhenIsHeaplessModeEnabledThen64BitAddressingIsAlwaysFalse) {
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

CRITEST_F(CriProductHelper, givenCompilerProductHelperWhenIsHeaplessStateInitEnabledThenCorrectValueIsSet) {
    DebugManagerStateRestore restorer;

    bool heaplessEnabled = false;
    debugManager.flags.Enable64bAddressingStateInit.set(-1);
    EXPECT_FALSE(compilerProductHelper->isHeaplessStateInitEnabled(heaplessEnabled));

    heaplessEnabled = true;
    EXPECT_TRUE(compilerProductHelper->isHeaplessStateInitEnabled(heaplessEnabled));

    debugManager.flags.Enable64bAddressingStateInit.set(0);
    EXPECT_FALSE(compilerProductHelper->isHeaplessStateInitEnabled(heaplessEnabled));

    debugManager.flags.Enable64bAddressingStateInit.set(1);
    EXPECT_TRUE(compilerProductHelper->isHeaplessStateInitEnabled(heaplessEnabled));
}

CRITEST_F(CriProductHelper, whenApplyExtraInternalOptionsIsCalledThenInternalOptionsAreCorrect) {
    DebugManagerStateRestore restorer;
    std::string enable64bitAddressing = "-ze-intel-64bit-addressing";
    {
        debugManager.flags.Enable64BitAddressing.set(-1);
        std::string internalOptions;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);
        EXPECT_TRUE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        debugManager.flags.Enable64BitAddressing.set(0);
        std::string internalOptions;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);
        EXPECT_FALSE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        debugManager.flags.Enable64BitAddressing.set(1);
        std::string internalOptions;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, NEO::CompilerOptions::HeaplessMode::defaultMode);
        EXPECT_TRUE(hasSubstr(internalOptions, enable64bitAddressing));
    }
}

CRITEST_F(CriProductHelper, givenHeaplessModeWhenApplyExtraInternalOptionsIsCalledThenInternalOptionsAreCorrect) {
    DebugManagerStateRestore restorer;
    std::string enable64bitAddressing = "-ze-intel-64bit-addressing";
    {
        debugManager.flags.Enable64BitAddressing.set(-1);
        std::string internalOptions;
        NEO::CompilerOptions::HeaplessMode heaplessMode = NEO::CompilerOptions::HeaplessMode::disabled;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, heaplessMode);
        EXPECT_FALSE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        debugManager.flags.Enable64BitAddressing.set(1);
        std::string internalOptions;
        NEO::CompilerOptions::HeaplessMode heaplessMode = NEO::CompilerOptions::HeaplessMode::disabled;

        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, heaplessMode);
        EXPECT_FALSE(hasSubstr(internalOptions, enable64bitAddressing));
    }
    {
        debugManager.flags.Enable64BitAddressing.set(0);
        std::string internalOptions;
        NEO::CompilerOptions::HeaplessMode heaplessMode = NEO::CompilerOptions::HeaplessMode::enabled;
        NEO::CompilerOptions::applyExtraInternalOptions(internalOptions, *defaultHwInfo, *compilerProductHelper, heaplessMode);
        EXPECT_TRUE(hasSubstr(internalOptions, enable64bitAddressing));
    }
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGettingDefaultCopyEngineThenEngineBCS1IsReturned) {
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS1, productHelper->getDefaultCopyEngine());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAdjustingEnginesGroupThenChangeEngineGroupToCopyFromLinkedCopyOnly) {
    for (uint32_t engineGroupIt = static_cast<uint32_t>(EngineGroupType::compute); engineGroupIt < static_cast<uint32_t>(EngineGroupType::maxEngineGroups); engineGroupIt++) {
        auto engineGroupType = static_cast<EngineGroupType>(engineGroupIt);
        auto engineGroupTypeUnchanged = engineGroupType;
        if (EngineGroupType::linkedCopy == engineGroupType) {
            productHelper->adjustEngineGroupType(engineGroupType);
            EXPECT_EQ(EngineGroupType::copy, engineGroupType);
        } else {
            productHelper->adjustEngineGroupType(engineGroupType);
            EXPECT_EQ(engineGroupTypeUnchanged, engineGroupType);
        }
    }
}

CRITEST_F(CriProductHelper, givenSmallRegionCountWhenAskingForLocalDispatchSizeThenReturnEmpty) {
    pInHwInfo.featureTable.regionCount = 1;

    const auto quantumSizes = productHelper->getSupportedLocalDispatchSizes(pInHwInfo);

    EXPECT_EQ(0u, quantumSizes.size());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenIsImplicitScalingSupportedThenExpectFalse) {
    EXPECT_TRUE(productHelper->isImplicitScalingSupported(*defaultHwInfo));
}

CRITEST_F(CriProductHelper, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingForDeviceToHostCopySignalingFenceTrueReturned) {
    EXPECT_TRUE(productHelper->isDeviceToHostCopySignalingFenceRequired());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAdjustNumberOfCcsThenOverrideToSingleCcs) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 123;
    productHelper->adjustNumberOfCcs(hwInfo);
    EXPECT_EQ(hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 1u);
}

CRITEST_F(CriProductHelper, givenEnableExtendedScratchSurfaceSizeFlagWhenCallIsAvailableExtendedScratchThenReturnProperValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(1);
    EXPECT_TRUE(productHelper->isAvailableExtendedScratch());

    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    EXPECT_FALSE(productHelper->isAvailableExtendedScratch());
}

CRITEST_F(CriProductHelper, givenPatIndexWhenCheckIsCoherentAllocationThenReturnProperValue) {
    std::array<uint64_t, 15> listOfCoherentPatIndexes = {1, 2, 4, 6, 7, 9, 10, 11, 22, 24, 25, 27, 28, 30, 31};
    for (auto patIndex : listOfCoherentPatIndexes) {
        EXPECT_TRUE(productHelper->isCoherentAllocation(patIndex).value());
    }
    std::array<uint64_t, 7> listOfNonCoherentPatIndexes = {0, 3, 5, 8, 23, 26, 29};
    for (auto patIndex : listOfNonCoherentPatIndexes) {
        EXPECT_FALSE(productHelper->isCoherentAllocation(patIndex).value());
    }
}

CRITEST_F(CriProductHelper, givenDefaultCacheRegionWhenGettingPatIndexThenZeroIsReturned) {
    auto allCachePolicies = {CachePolicy::uncached, CachePolicy::writeCombined, CachePolicy::writeThrough, CachePolicy::writeBack};
    for (auto &cachePolicy : allCachePolicies) {
        EXPECT_EQ(0u, productHelper->getPatIndex(CacheRegion::defaultRegion, cachePolicy));
    }
}

CRITEST_F(CriProductHelper, givenRegion1WhenGettingPatIndexThenProperValueIsReturned) {
    auto allCachePolicies = {CachePolicy::uncached, CachePolicy::writeCombined, CachePolicy::writeThrough, CachePolicy::writeBack};
    for (auto &cachePolicy : allCachePolicies) {
        EXPECT_EQ(23u, productHelper->getPatIndex(CacheRegion::region1, cachePolicy));
    }
}

CRITEST_F(CriProductHelper, givenRegion2WhenGettingPatIndexThenProperValueIsReturned) {
    auto allCachePolicies = {CachePolicy::uncached, CachePolicy::writeCombined, CachePolicy::writeThrough, CachePolicy::writeBack};
    for (auto &cachePolicy : allCachePolicies) {
        EXPECT_EQ(24u, productHelper->getPatIndex(CacheRegion::region2, cachePolicy));
    }
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGettingAreSecondaryContextsSupportedThenExpectTrue) {
    EXPECT_TRUE(productHelper->areSecondaryContextsSupported());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenCheckingIs2MBLocalMemAlignmentEnabledThenCorrectValueIsReturned) {
    EXPECT_TRUE(productHelper->is2MBLocalMemAlignmentEnabled());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingForSharingWith3dOrMediaSupportThenFalseReturned) {
    EXPECT_FALSE(productHelper->isSharingWith3dOrMediaAllowed());
}

CRITEST_F(CriProductHelper, givenGrfCount512WhenCallAdjustMaxThreadsPerThreadGroupThenAdjustThreadsPerThreadGroup) {
    uint32_t threadsPerThreadGroup = 22;
    uint32_t expectedMaxThreadsPerThreadGroup = 32u;
    std::array<std::array<uint32_t, 2>, 4> values = {{{32, true}, // simt, isHeaplessModeEnabled
                                                      {32, false},
                                                      {16, true},
                                                      {16, false}}};
    for (auto &[simt, isHeaplessModeEnabled] : values) {
        EXPECT_EQ(expectedMaxThreadsPerThreadGroup, productHelper->adjustMaxThreadsPerThreadGroup(threadsPerThreadGroup, simt, 512, isHeaplessModeEnabled));
    }
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingShouldRegisterEnqueuedWalkerWithProfilingThenTrueReturned) {
    EXPECT_TRUE(productHelper->shouldRegisterEnqueuedWalkerWithProfiling());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingIsInterruptSupportedThenTrueReturned) {
    EXPECT_TRUE(productHelper->isInterruptSupported());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingIsMediaContextSupportedThenTrueReturned) {
    EXPECT_TRUE(productHelper->isMediaContextSupported());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenCheckingInitializeInternalEngineImmediatelyThenCorrectValueIsReturned) {
    EXPECT_FALSE(productHelper->initializeInternalEngineImmediately());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGettingPreferredWorkgroupCountPerSubsliceThenFourIsReturned) {
    EXPECT_EQ(4u, productHelper->getPreferredWorkgroupCountPerSubslice());
}