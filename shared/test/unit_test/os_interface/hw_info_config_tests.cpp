/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "gtest/gtest.h"

using namespace NEO;

ProductHelperTest::ProductHelperTest() {
    executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    productHelper = &executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
}

ProductHelperTest::~ProductHelperTest() = default;

void ProductHelperTest::SetUp() {
    pInHwInfo = *defaultHwInfo;
    testPlatform = &pInHwInfo.platform;
}

HWTEST_F(ProductHelperTest, givenDebugFlagSetWhenAskingForHostMemCapabilitesThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    DebugManager.flags.EnableHostUsmSupport.set(0);
    EXPECT_EQ(0u, productHelper->getHostMemCapabilities(&pInHwInfo));

    DebugManager.flags.EnableHostUsmSupport.set(1);
    EXPECT_NE(0u, productHelper->getHostMemCapabilities(&pInHwInfo));
}

HWTEST_F(ProductHelperTest, whenGettingDefaultRevisionIdThenZeroIsReturned) {
    EXPECT_EQ(0u, productHelper->getDefaultRevisionId());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingSharedSystemMemCapabilitiesThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;

    EXPECT_EQ(0u, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));

    for (auto enable : {-1, 0, 1}) {
        DebugManager.flags.EnableSharedSystemUsmSupport.set(enable);

        if (enable > 0) {
            auto caps = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
            EXPECT_EQ(caps, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));
        } else {
            EXPECT_EQ(0u, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));
        }
    }
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfIsBlitSplitEnqueueWARequiredThenReturnFalse) {

    EXPECT_FALSE(productHelper->isBlitSplitEnqueueWARequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingMemoryCapabilitiesThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;

    for (auto capabilityBitmask : {0, 0b0001, 0b0010, 0b0100, 0b1000, 0b1111}) {
        DebugManager.flags.EnableUsmConcurrentAccessSupport.set(capabilityBitmask);
        std::bitset<4> capabilityBitset(capabilityBitmask);

        auto hostMemCapabilities = productHelper->getHostMemCapabilities(&pInHwInfo);
        if (hostMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::Host))) {
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS & hostMemCapabilities);
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS & hostMemCapabilities);
            }
        }

        auto deviceMemCapabilities = productHelper->getDeviceMemCapabilities();
        if (deviceMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::Device))) {
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS & deviceMemCapabilities);
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS & deviceMemCapabilities);
            }
        }

        auto singleDeviceSharedMemCapabilities = productHelper->getSingleDeviceSharedMemCapabilities();
        if (singleDeviceSharedMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::SharedSingleDevice))) {
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS & singleDeviceSharedMemCapabilities);
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS & singleDeviceSharedMemCapabilities);
            }
        }

        auto crossDeviceSharedMemCapabilities = productHelper->getCrossDeviceSharedMemCapabilities();
        if (crossDeviceSharedMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::SharedCrossDevice))) {
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS & crossDeviceSharedMemCapabilities);
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS & crossDeviceSharedMemCapabilities);
            }
        }

        auto sharedSystemMemCapabilities = productHelper->getSharedSystemMemCapabilities(&pInHwInfo);
        if (sharedSystemMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::SharedSystemCrossDevice))) {
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS & sharedSystemMemCapabilities);
                EXPECT_TRUE(UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS & sharedSystemMemCapabilities);
            }
        }
    }
}

TEST_F(ProductHelperTest, WhenParsingHwInfoConfigThenCorrectValuesAreReturned) {
    uint64_t hwInfoConfig = 0x0;

    bool success = parseHwInfoConfigString("1x1x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100010001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 1u);
    for (uint32_t slice = 0; slice < outHwInfo.gtSystemInfo.SliceCount; slice++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[slice].Enabled);
    }

    success = parseHwInfoConfigString("3x1x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x300010001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 3u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 3u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 3u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 3u);
    for (uint32_t slice = 0; slice < outHwInfo.gtSystemInfo.SliceCount; slice++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[slice].Enabled);
    }

    success = parseHwInfoConfigString("1x7x1", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100070001u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 7u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 7u);
    for (uint32_t slice = 0; slice < outHwInfo.gtSystemInfo.SliceCount; slice++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[slice].Enabled);
    }

    success = parseHwInfoConfigString("1x1x7", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(hwInfoConfig, 0x100010007u);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 1u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 7u);
    for (uint32_t slice = 0; slice < outHwInfo.gtSystemInfo.SliceCount; slice++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[slice].Enabled);
    }

    success = parseHwInfoConfigString("2x4x16", hwInfoConfig);
    EXPECT_TRUE(success);
    EXPECT_EQ(0x200040010u, hwInfoConfig);
    setHwInfoValuesFromConfig(hwInfoConfig, outHwInfo);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SliceCount, 2u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.SubSliceCount, 8u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.DualSubSliceCount, 8u);
    EXPECT_EQ(outHwInfo.gtSystemInfo.EUCount, 128u);
    for (uint32_t slice = 0; slice < outHwInfo.gtSystemInfo.SliceCount; slice++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[slice].Enabled);
    }
}

TEST_F(ProductHelperTest, givenInvalidHwInfoWhenParsingHwInfoConfigThenErrorIsReturned) {
    uint64_t hwInfoConfig = 0x0;
    bool success = parseHwInfoConfigString("1", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x3", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("65536x3x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x65536x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x3x65536", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("65535x65535x8", hwInfoConfig);
    EXPECT_FALSE(success);

    success = parseHwInfoConfigString("1x65535x65535", hwInfoConfig);
    EXPECT_FALSE(success);
}

HWTEST_F(ProductHelperTest, whenOverrideGfxPartitionLayoutForWslThenReturnFalse) {

    EXPECT_FALSE(productHelper->overrideGfxPartitionLayoutForWsl());
}

HWTEST_F(ProductHelperTest, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned) {

    bool ret = productHelper->isAdditionalStateBaseAddressWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}

HWTEST_F(ProductHelperTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned) {

    bool ret = productHelper->isMaxThreadsForWorkgroupWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedForPageTableManagerSupportThenReturnCorrectValue) {

    EXPECT_EQ(productHelper->isPageTableManagerSupported(pInHwInfo), UnitTestHelper<FamilyType>::isPageTableManagerSupported(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect) {

    for (uint32_t testValue = 0; testValue < 0x10; testValue++) {
        auto hwRevIdFromStepping = productHelper->getHwRevIdFromStepping(testValue, pInHwInfo);
        if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
            pInHwInfo.platform.usRevId = hwRevIdFromStepping;
            EXPECT_EQ(testValue, productHelper->getSteppingFromHwRevId(pInHwInfo));
        }
        pInHwInfo.platform.usRevId = testValue;
        auto steppingFromHwRevId = productHelper->getSteppingFromHwRevId(pInHwInfo);
        if (steppingFromHwRevId != CommonConstants::invalidStepping) {
            EXPECT_EQ(testValue, productHelper->getHwRevIdFromStepping(steppingFromHwRevId, pInHwInfo));
        }
    }
}

HWTEST_F(ProductHelperTest, givenVariousValuesWhenGettingAubStreamSteppingFromHwRevIdThenReturnValuesAreCorrect) {
    MockProductHelperHw<IGFX_UNKNOWN> mockProductHelper;
    mockProductHelper.returnedStepping = REVISION_A0;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_A1;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_A3;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_B;
    EXPECT_EQ(AubMemDump::SteppingValues::B, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_C;
    EXPECT_EQ(AubMemDump::SteppingValues::C, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_D;
    EXPECT_EQ(AubMemDump::SteppingValues::D, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = REVISION_K;
    EXPECT_EQ(AubMemDump::SteppingValues::K, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockProductHelper.returnedStepping = CommonConstants::invalidStepping;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockProductHelper.getAubStreamSteppingFromHwRevId(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedForDefaultEngineTypeAdjustmentThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isDefaultEngineTypeAdjustmentRequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, whenCallingGetDeviceMemoryNameThenDdrIsReturned) {

    auto deviceMemoryName = productHelper->getDeviceMemoryName();
    EXPECT_TRUE(hasSubstr(deviceMemoryName, std::string("DDR")));
}

HWCMDTEST_F(IGFX_GEN8_CORE, ProductHelperTest, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    EXPECT_FALSE(productHelper->isDisableOverdispatchAvailable(pInHwInfo));
}

HWTEST_F(ProductHelperTest, WhenAllowRenderCompressionIsCalledThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->allowCompression(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenVariousDebugKeyValuesWhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore{};

    struct MockProductHelper : ProductHelperHw<IGFX_UNKNOWN> {
        using ProductHelper::getDefaultLocalMemoryAccessMode;
    };
    auto mockProductHelper = static_cast<MockProductHelper &>(*ProductHelper::get(productFamily));
    EXPECT_EQ(mockProductHelper.getDefaultLocalMemoryAccessMode(pInHwInfo), mockProductHelper.getLocalMemoryAccessMode(pInHwInfo));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_EQ(LocalMemoryAccessMode::Default, productHelper->getLocalMemoryAccessMode(pInHwInfo));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessAllowed, productHelper->getLocalMemoryAccessMode(pInHwInfo));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, productHelper->getLocalMemoryAccessMode(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfAllocationSizeAdjustmentIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isAllocationSizeAdjustmentRequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPrefetchDisablingIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isPrefetchDisablingRequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned) {

    auto isRcs = false;
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(pInHwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_FALSE(isBasicWARequired);
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfHeapInLocalMemThenFalseIsReturned, IsAtMostGen12lp) {

    EXPECT_FALSE(productHelper->heapInLocalMem(pInHwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenSettingCapabilityCoherencyFlagThenFlagIsSet, IsAtMostGen11) {

    bool coherency = false;
    productHelper->setCapabilityCoherencyFlag(pInHwInfo, coherency);
    EXPECT_TRUE(coherency);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfAdditionalMediaSamplerProgrammingIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isAdditionalMediaSamplerProgrammingRequired());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfInitialFlagsProgrammingIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isInitialFlagsProgrammingRequired());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfReturnedCmdSizeForMediaSamplerAdjustmentIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isReturnedCmdSizeForMediaSamplerAdjustmentRequired());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfExtraParametersAreInvalidThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->extraParametersInvalid(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlWAIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->pipeControlWARequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfImagePitchAlignmentWAIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->imagePitchAlignmentWARequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfForceEmuInt32DivRemSPWAIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isForceEmuInt32DivRemSPWARequired(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIf3DPipelineSelectWAIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->is3DPipelineSelectWARequired());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfStorageInfoAdjustmentIsRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isStorageInfoAdjustmentRequired());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfBlitterForImagesIsSupportedThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isBlitterForImagesSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->isTile64With3DSurfaceOnBCSSupported(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse) {

    EXPECT_FALSE(productHelper->isVmBindPatIndexProgrammingSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfIsTimestampWaitSupportedForEventsThenFalseIsReturned, IsNotXeHpgOrXeHpcCore) {

    EXPECT_FALSE(productHelper->isTimestampWaitSupportedForEvents());
}

HWTEST_F(ProductHelperTest, givenLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};

    pInHwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);

    auto expectedDefaultValue = (productHelper->getLocalMemoryAccessMode(pInHwInfo) == LocalMemoryAccessMode::CpuAccessDisallowed);
    EXPECT_EQ(expectedDefaultValue, productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));
    pInHwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::System64KBPages);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));
    pInHwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));
}

HWTEST_F(ProductHelperTest, givenNotLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};
    HardwareInfo hwInfo = pInHwInfo;

    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::SVM_GPU);
    EXPECT_FALSE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.initGmm();
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();

    MockGmm mockGmm(gmmHelper, nullptr, 100, 100, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true);
    mockGmm.resourceParams.Flags.Info.NotLockable = true;
    graphicsAllocation.setDefaultGmm(&mockGmm);

    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::System64KBPages);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(hwInfo, graphicsAllocation));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenGettingIsBlitCopyRequiredForLocalMemoryThenFalseIsReturned, IsAtMostGen11) {

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.overrideMemoryPool(MemoryPool::LocalMemory);
    graphicsAllocation.setAllocationType(AllocationType::BUFFER_HOST_MEMORY);

    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(pInHwInfo, graphicsAllocation));
}

HWTEST_F(ProductHelperTest, givenSamplerStateWhenAdjustSamplerStateThenNothingIsChanged) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    auto state = FamilyType::cmdInitSamplerState;
    auto initialState = state;
    productHelper->adjustSamplerState(&state, pInHwInfo);

    EXPECT_EQ(0, memcmp(&initialState, &state, sizeof(SAMPLER_STATE)));
}

HWTEST_F(ProductHelperTest, WhenFillingScmPropertiesSupportThenExpectUseCorrectGetters) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};

    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(productHelper->isThreadArbitrationPolicyReportedWithScm(), scmPropertiesSupport.threadArbitrationPolicy);
    EXPECT_EQ(productHelper->getScmPropertyCoherencyRequiredSupport(), scmPropertiesSupport.coherencyRequired);
    EXPECT_EQ(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport(), scmPropertiesSupport.zPassAsyncComputeThreadLimit);
    EXPECT_EQ(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport(), scmPropertiesSupport.pixelAsyncComputeThreadLimit);
    EXPECT_EQ(productHelper->isGrfNumReportedWithScm(), scmPropertiesSupport.largeGrfMode);
    EXPECT_EQ(productHelper->getScmPropertyDevicePreemptionModeSupport(), scmPropertiesSupport.devicePreemptionMode);
}

HWTEST_F(ProductHelperTest, WhenFillingFrontEndPropertiesSupportThenExpectUseCorrectGetters) {
    FrontEndPropertiesSupport frontEndPropertiesSupport = {};

    productHelper->fillFrontEndPropertiesSupportStructure(frontEndPropertiesSupport, pInHwInfo);
    EXPECT_EQ(productHelper->isComputeDispatchAllWalkerEnableInCfeStateRequired(pInHwInfo), frontEndPropertiesSupport.computeDispatchAllWalker);
    EXPECT_EQ(productHelper->getFrontEndPropertyDisableEuFusionSupport(), frontEndPropertiesSupport.disableEuFusion);
    EXPECT_EQ(productHelper->isDisableOverdispatchAvailable(pInHwInfo), frontEndPropertiesSupport.disableOverdispatch);
    EXPECT_EQ(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport(), frontEndPropertiesSupport.singleSliceDispatchCcsMode);
}

HWTEST_F(ProductHelperTest, WhenFillingPipelineSelectPropertiesSupportThenExpectUseCorrectGetters) {
    PipelineSelectPropertiesSupport pipelineSelectPropertiesSupport = {};

    productHelper->fillPipelineSelectPropertiesSupportStructure(pipelineSelectPropertiesSupport, pInHwInfo);
    EXPECT_EQ(productHelper->getPipelineSelectPropertyModeSelectedSupport(), pipelineSelectPropertiesSupport.modeSelected);
    EXPECT_EQ(productHelper->getPipelineSelectPropertyMediaSamplerDopClockGateSupport(), pipelineSelectPropertiesSupport.mediaSamplerDopClockGate);
    EXPECT_EQ(productHelper->isSystolicModeConfigurable(pInHwInfo), pipelineSelectPropertiesSupport.systolicMode);
}

HWTEST_F(ProductHelperTest, WhenFillingStateBaseAddressPropertiesSupportThenExpectUseCorrectGetters) {
    StateBaseAddressPropertiesSupport stateBaseAddressPropertiesSupport = {};

    productHelper->fillStateBaseAddressPropertiesSupportStructure(stateBaseAddressPropertiesSupport, pInHwInfo);
    EXPECT_EQ(productHelper->getStateBaseAddressPropertyGlobalAtomicsSupport(), stateBaseAddressPropertiesSupport.globalAtomics);
    EXPECT_EQ(productHelper->getStateBaseAddressPropertyStatelessMocsSupport(), stateBaseAddressPropertiesSupport.statelessMocs);
    EXPECT_EQ(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport(), stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsAdjustProgrammableIdPreferredSlmSizeRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isAdjustProgrammableIdPreferredSlmSizeRequired(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsComputeDispatchAllWalkerEnableInCfeStateRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isComputeDispatchAllWalkerEnableInCfeStateRequired(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsComputeDispatchAllWalkerEnableInComputeWalkerRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isComputeDispatchAllWalkerEnableInComputeWalkerRequired(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsGlobalFenceInCommandStreamRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isGlobalFenceInCommandStreamRequired(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsSystolicModeConfigurabledThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isSystolicModeConfigurable(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGetThreadEuRatioForScratchThen8IsReturned) {

    EXPECT_EQ(8u, productHelper->getThreadEuRatioForScratch(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenDefaultSettingWhenIsGrfNumReportedIsCalledThenScmSupportProductValueIsReturned) {

    EXPECT_EQ(productHelper->getScmPropertyLargeGrfModeSupport(), productHelper->isGrfNumReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenForceGrfNumProgrammingWithScmFlagSetWhenIsGrfNumReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(0);
    EXPECT_FALSE(productHelper->isGrfNumReportedWithScm());

    DebugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    EXPECT_TRUE(productHelper->isGrfNumReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenDefaultSettingWhenIsThreadArbitrationPolicyReportedIsCalledThenScmSupportProductValueReturned) {

    EXPECT_EQ(productHelper->getScmPropertyThreadArbitrationPolicySupport(), productHelper->isThreadArbitrationPolicyReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenForceThreadArbitrationPolicyProgrammingWithScmFlagSetWhenIsThreadArbitrationPolicyReportedWithScmIsQueriedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(0);
    EXPECT_FALSE(productHelper->isThreadArbitrationPolicyReportedWithScm());

    DebugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);
    EXPECT_TRUE(productHelper->isThreadArbitrationPolicyReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsAdjustWalkOrderAvailableCallThenFalseReturn) {
    EXPECT_FALSE(productHelper->isAdjustWalkOrderAvailable(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isPrefetcherDisablingInDirectSubmissionRequired());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsImplicitScalingSupportedThenExpectFalse, IsNotXeHpOrXeHpcCore) {
    EXPECT_FALSE(productHelper->isImplicitScalingSupported(*defaultHwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperAndDebugFlagWhenGetL1CachePolicyThenReturnCorrectPolicy, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, productHelper->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WB, productHelper->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WT, productHelper->getL1CachePolicy(true));

    DebugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WS, productHelper->getL1CachePolicy(true));

    DebugManager.flags.ForceAllResourcesUncached.set(true);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_UC, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenGetL1CachePolicyThenReturnWriteByPass, IsAtLeastXeHpgCore) {
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_POLICY_WBP, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenPlatformWithUnsupportedL1CachePoliciesWhenGetL1CachePolicyThenReturnZero, IsAtMostXeHpCore) {
    EXPECT_EQ(0u, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(0u, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsStatefulAddressingModeSupportedThenReturnTrue, HasStatefulSupport) {
    EXPECT_TRUE(productHelper->isStatefulAddressingModeSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsPlatformQueryNotSupportedThenReturnFalse, IsAtMostDg2) {
    EXPECT_FALSE(productHelper->isPlatformQuerySupported());
}

HWTEST_F(ProductHelperTest, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    auto productHelper = ProductHelper::get(pInHwInfo.platform.eProductFamily);

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true));

    DebugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned) {
    EXPECT_FALSE(productHelper->isBufferPoolAllocatorSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIsMultiContextResourceDeferDeletionSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isMultiContextResourceDeferDeletionSupported());
}
