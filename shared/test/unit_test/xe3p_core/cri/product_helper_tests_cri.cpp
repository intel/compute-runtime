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
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/kernel/kernel_properties.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/unified_memory/unified_memory.h"
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

    EXPECT_TRUE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));
}

CRITEST_F(CriProductHelper, givenFtrHeaplessModeFalseWhenIsHeaplessModeEnabledThen64BitAddressingIsAlwaysFalse) {
    DebugManagerStateRestore restorer;
    VariableBackup<FeatureTableBase::Flags> ftrHeaplessModeBackup{&defaultHwInfo->featureTable.flags};
    defaultHwInfo->featureTable.flags.ftrHeaplessMode = false;

    EXPECT_FALSE(compilerProductHelper->isHeaplessModeEnabled(*defaultHwInfo));
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
    std::array<uint64_t, 16> listOfCoherentPatIndexes = {1, 2, 4, 6, 7, 9, 10, 11, 19, 22, 24, 25, 27, 28, 30, 31};
    for (auto patIndex : listOfCoherentPatIndexes) {
        EXPECT_TRUE(productHelper->isCoherentAllocation(patIndex).value());
    }
    std::array<uint64_t, 8> listOfNonCoherentPatIndexes = {0, 3, 5, 8, 18, 23, 26, 29};
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
    std::array<uint32_t, 2> values = {32, 16};
    for (auto simt : values) {
        EXPECT_EQ(expectedMaxThreadsPerThreadGroup, productHelper->adjustMaxThreadsPerThreadGroup(threadsPerThreadGroup, simt, 512));
    }
}

CRITEST_F(CriProductHelper, givenGrfCount160Or192WhenCallAdjustMaxThreadsPerThreadGroupThenAdjustOnlyForSimd16AndSimd1) {
    constexpr uint32_t threadsPerThreadGroup = 40u;
    struct TestCase {
        uint32_t simt;
        uint32_t grfCount;
        uint32_t expectedMaxThreadsPerThreadGroup;
    };
    constexpr std::array<TestCase, 12> testCases = {{{1u, 160u, 64u},
                                                     {16u, 160u, 64u},
                                                     {32u, 160u, threadsPerThreadGroup},
                                                     {1u, 192u, 64u},
                                                     {16u, 192u, 64u},
                                                     {32u, 192u, threadsPerThreadGroup},
                                                     {1u, 128u, threadsPerThreadGroup},
                                                     {16u, 128u, threadsPerThreadGroup},
                                                     {32u, 128u, threadsPerThreadGroup},
                                                     {1u, 256u, threadsPerThreadGroup},
                                                     {16u, 256u, threadsPerThreadGroup},
                                                     {32u, 256u, threadsPerThreadGroup}}};
    for (const auto &testCase : testCases) {
        EXPECT_EQ(testCase.expectedMaxThreadsPerThreadGroup, productHelper->adjustMaxThreadsPerThreadGroup(threadsPerThreadGroup, testCase.simt, testCase.grfCount));
    }
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingShouldRegisterEnqueuedWalkerWithProfilingThenTrueReturned) {
    EXPECT_TRUE(productHelper->shouldRegisterEnqueuedWalkerWithProfiling());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenCheckingInitializeInternalEngineImmediatelyThenCorrectValueIsReturned) {
    EXPECT_FALSE(productHelper->initializeInternalEngineImmediately());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGettingPreferredWorkgroupCountPerSubsliceThenFourIsReturned) {
    EXPECT_EQ(4u, productHelper->getPreferredWorkgroupCountPerSubslice());
}

CRITEST_F(CriProductHelper, givenAtLeastXe3pCoreWhenGetL1CachePolicyThenReturnWB) {
    EXPECT_EQ(productHelper->getL1CachePolicy(false), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WB);
    EXPECT_EQ(productHelper->getL1CachePolicy(true), FamilyType::RENDER_SURFACE_STATE::L1_CACHE_CONTROL_WBP);
}

CRITEST_F(CriProductHelper, givenProductHelperWhenCheckingIsLEOSupportedThenReturnTrue) {
    EXPECT_TRUE(productHelper->isLEOSupported());
}

CRITEST_F(CriProductHelper, givenProductHelperWhenGetCpuCopyThresholdThenReturnCriThresholds) {
    EXPECT_EQ(0u, productHelper->getCpuCopyThreshold(TransferType::unknown));

    EXPECT_EQ(4 * MemoryConstants::kiloByte, productHelper->getCpuCopyThreshold(TransferType::deviceUsmToDeviceUsm));
    EXPECT_EQ(4 * MemoryConstants::kiloByte, productHelper->getCpuCopyThreshold(TransferType::deviceUsmToHostUsm));
    EXPECT_EQ(64 * MemoryConstants::kiloByte, productHelper->getCpuCopyThreshold(TransferType::deviceUsmToHostNonUsm));

    EXPECT_EQ(64 * MemoryConstants::kiloByte, productHelper->getCpuCopyThreshold(TransferType::hostUsmToDeviceUsm));
    EXPECT_EQ(1 * MemoryConstants::megaByte, productHelper->getCpuCopyThreshold(TransferType::hostUsmToHostUsm));
    EXPECT_EQ(64 * MemoryConstants::megaByte, productHelper->getCpuCopyThreshold(TransferType::hostUsmToHostNonUsm));

    EXPECT_EQ(10 * MemoryConstants::megaByte, productHelper->getCpuCopyThreshold(TransferType::hostNonUsmToDeviceUsm));
    EXPECT_EQ(64 * MemoryConstants::megaByte, productHelper->getCpuCopyThreshold(TransferType::hostNonUsmToHostUsm));
    EXPECT_EQ(64 * MemoryConstants::megaByte, productHelper->getCpuCopyThreshold(TransferType::hostNonUsmToHostNonUsm));

    EXPECT_EQ(0u, productHelper->getCpuCopyThreshold(TransferType::sharedUsmToSharedUsm));
}

CRITEST_F(CriProductHelper, givenProductHelperWhenAskingForSupportedRtasFormatThenCorrectFormatIsReturned) {
    EXPECT_EQ(RTASDeviceFormat::version2, productHelper->getSupportedRtasFormat());
}
