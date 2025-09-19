/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/definitions/engine_group_types.h"
#include "shared/source/helpers/definitions/indirect_detection_versions.h"
#include "shared/source/helpers/device_caps_reader.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/kernel/kernel_descriptor.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/unified_memory/usm_memory_support.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/device_caps_reader_test_helper.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_driver_model.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "clos_matchers.h"
#include "gtest/gtest.h"
#include "ocl_igc_shared/indirect_access_detection/version.h"
#include "test_traits_common.h"

namespace NEO {
extern ApiSpecificConfig::ApiType apiTypeForUlts;
}

using namespace NEO;

ProductHelperTest::ProductHelperTest() {
    executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    productHelper = &executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    compilerProductHelper = &executionEnvironment->rootDeviceEnvironments[0]->getHelper<CompilerProductHelper>();
    releaseHelper = executionEnvironment->rootDeviceEnvironments[0]->getReleaseHelper();
}

ProductHelperTest::~ProductHelperTest() = default;

void ProductHelperTest::SetUp() {
    pInHwInfo = *defaultHwInfo;
    testPlatform = &pInHwInfo.platform;
}

void ProductHelperTest::refreshReleaseHelper(HardwareInfo *hwInfo) {
    executionEnvironment->rootDeviceEnvironments[0]->releaseHelper = executionEnvironment->rootDeviceEnvironments[0]->releaseHelper->create(hwInfo->ipVersion);
    releaseHelper = executionEnvironment->rootDeviceEnvironments[0]->getReleaseHelper();
}

TEST(ProductHelperTestCreate, WhenProductHelperCreateIsCalledWithUnknownProductThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, ProductHelper::create(IGFX_UNKNOWN));
}

HWTEST_F(ProductHelperTest, givenDriverModelWhenGetDeviceCapsReaderIsCalledThenNullptrIsReturned) {
    auto driverModel = std::make_unique<MockDriverModel>();

    auto capsReader = productHelper->getDeviceCapsReader(*driverModel);
    EXPECT_EQ(nullptr, capsReader);
}

HWTEST_F(ProductHelperTest, givenAubManagerWhenGetDeviceCapsReaderIsCalledThenNullptrIsReturned) {
    auto aubManager = std::make_unique<MockAubManager>();

    auto capsReader = productHelper->getDeviceCapsReader(*aubManager);
    EXPECT_EQ(nullptr, capsReader);
}

HWTEST_F(ProductHelperTest, whenSetupHardwareInfoIsCalledThenTrueIsReturned) {
    std::vector<uint32_t> caps;
    DeviceCapsReaderMock capsReader(caps);

    auto ret = productHelper->setupHardwareInfo(pInHwInfo, capsReader);
    EXPECT_EQ(true, ret);
}

HWTEST_F(ProductHelperTest, givenDebugFlagSetWhenAskingForHostMemCapabilitiesThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnableHostUsmSupport.set(0);
    EXPECT_EQ(0u, productHelper->getHostMemCapabilities(&pInHwInfo));

    debugManager.flags.EnableHostUsmSupport.set(1);
    EXPECT_NE(0u, productHelper->getHostMemCapabilities(&pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingSharedSystemMemCapabilitiesThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;

    uint64_t caps = (UnifiedSharedMemoryFlags::access | UnifiedSharedMemoryFlags::atomicAccess | UnifiedSharedMemoryFlags::concurrentAccess | UnifiedSharedMemoryFlags::concurrentAtomicAccess);
    pInHwInfo.capabilityTable.sharedSystemMemCapabilities = caps;

    for (auto enable : {-1, 0, 1}) {
        debugManager.flags.EnableSharedSystemUsmSupport.set(enable);
        if (enable != 1) {
            EXPECT_EQ(0u, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));
        } else {
            for (auto pfEnable : {-1, 0, 1}) {
                debugManager.flags.EnableRecoverablePageFaults.set(pfEnable);
                if (pfEnable != 0) {
                    EXPECT_EQ(caps, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));
                } else {
                    EXPECT_EQ(0u, productHelper->getSharedSystemMemCapabilities(&pInHwInfo));
                }
            }
        }
    }

    pInHwInfo.capabilityTable.sharedSystemMemCapabilities = caps;
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfIsBlitSplitEnqueueWARequiredThenReturnFalse) {

    EXPECT_FALSE(productHelper->getBcsSplitSettings(pInHwInfo).enabled);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingMemoryCapabilitiesThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;

    for (auto capabilityBitmask : {0, 0b0001, 0b0010, 0b0100, 0b1000, 0b1111}) {
        debugManager.flags.EnableUsmConcurrentAccessSupport.set(capabilityBitmask);
        std::bitset<4> capabilityBitset(capabilityBitmask);

        auto hostMemCapabilities = productHelper->getHostMemCapabilities(&pInHwInfo);
        if (hostMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::host))) {
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAccess & hostMemCapabilities);
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAtomicAccess & hostMemCapabilities);
            }
        }

        auto deviceMemCapabilities = productHelper->getDeviceMemCapabilities();
        if (deviceMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::device))) {
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAccess & deviceMemCapabilities);
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAtomicAccess & deviceMemCapabilities);
            }
        }

        constexpr bool isKmdMigrationAvailable{false};
        auto singleDeviceSharedMemCapabilities = productHelper->getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable);
        if (singleDeviceSharedMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::sharedSingleDevice))) {
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAccess & singleDeviceSharedMemCapabilities);
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAtomicAccess & singleDeviceSharedMemCapabilities);
            }
        }

        auto crossDeviceSharedMemCapabilities = productHelper->getCrossDeviceSharedMemCapabilities();
        if (crossDeviceSharedMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::sharedCrossDevice))) {
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAccess & crossDeviceSharedMemCapabilities);
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAtomicAccess & crossDeviceSharedMemCapabilities);
            }
        }

        auto sharedSystemMemCapabilities = productHelper->getSharedSystemMemCapabilities(&pInHwInfo);
        if (sharedSystemMemCapabilities > 0) {
            if (capabilityBitset.test(static_cast<uint32_t>(UsmAccessCapabilities::sharedSystemCrossDevice))) {
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAccess & sharedSystemMemCapabilities);
                EXPECT_TRUE(UnifiedSharedMemoryFlags::concurrentAtomicAccess & sharedSystemMemCapabilities);
            }
        }
    }
}

HWTEST_F(ProductHelperTest, givenProductHelperAndSingleDeviceSharedMemAccessConcurrentAtomicEnabledIfKmdMigrationEnabled) {
    DebugManagerStateRestore restore;
    debugManager.flags.EnableUsmConcurrentAccessSupport.set(0);

    for (const bool isKmdMigrationAvailable : std::array<bool, 2>{false, true}) {
        auto singleDeviceSharedMemCapabilities = productHelper->getSingleDeviceSharedMemCapabilities(isKmdMigrationAvailable);
        EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::access), UnifiedSharedMemoryFlags::access);
        EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::atomicAccess), UnifiedSharedMemoryFlags::atomicAccess);
        if (isKmdMigrationAvailable) {
            EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::concurrentAccess), UnifiedSharedMemoryFlags::concurrentAccess);
            EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::concurrentAtomicAccess), UnifiedSharedMemoryFlags::concurrentAtomicAccess);
        } else {
            EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::concurrentAccess), 0UL);
            EXPECT_EQ((singleDeviceSharedMemCapabilities & UnifiedSharedMemoryFlags::concurrentAtomicAccess), 0UL);
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

struct PlatformsWithReleaseHelper {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        return IsAtLeastMtl::isMatched<productFamily>() && !IsXeHpcCore::isMatched<productFamily>();
    }
};

HWTEST2_F(ProductHelperTest, whenPlatformsSupportsReleaseHelperThenRevIdFromSteppingsAreNotAvailable, PlatformsWithReleaseHelper) {
    ASSERT_NE(nullptr, releaseHelper);
    for (uint32_t testValue = 0; testValue < 0x10; testValue++) {
        EXPECT_EQ(CommonConstants::invalidStepping, productHelper->getHwRevIdFromStepping(testValue, pInHwInfo));
        pInHwInfo.platform.usRevId = testValue;
        EXPECT_EQ(CommonConstants::invalidStepping, productHelper->getSteppingFromHwRevId(pInHwInfo));
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, ProductHelperTest, givenProductHelperWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {

    EXPECT_FALSE(productHelper->isDisableOverdispatchAvailable(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenVariousDebugKeyValuesWhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore{};

    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_EQ(LocalMemoryAccessMode::defaultMode, productHelper->getLocalMemoryAccessMode(pInHwInfo));
    debugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_EQ(LocalMemoryAccessMode::cpuAccessAllowed, productHelper->getLocalMemoryAccessMode(pInHwInfo));
    debugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_EQ(LocalMemoryAccessMode::cpuAccessDisallowed, productHelper->getLocalMemoryAccessMode(pInHwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned, IsNotXeCore) {

    auto isRcs = false;
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper->isPipeControlPriorToNonPipelinedStateCommandsWARequired(pInHwInfo, isRcs, releaseHelper);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_FALSE(isBasicWARequired);
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfHeapInLocalMemThenFalseIsReturned, IsGen12LP) {

    EXPECT_FALSE(productHelper->heapInLocalMem(pInHwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsSkippingStatefulInformationRequiredThenFalseIsReturned, IsNotPVC) {

    KernelDescriptor kernelDescriptor{};
    kernelDescriptor.kernelMetadata.isGeneratedByIgc = true;
    EXPECT_FALSE(productHelper->isSkippingStatefulInformationRequired(kernelDescriptor));

    kernelDescriptor.kernelMetadata.isGeneratedByIgc = false;
    EXPECT_FALSE(productHelper->isSkippingStatefulInformationRequired(kernelDescriptor));
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

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCallUseGemCreateExtInAllocateMemoryByKMDThenFalseIsReturned, IsGen12LP) {
    EXPECT_FALSE(productHelper->useGemCreateExtInAllocateMemoryByKMD());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfBlitterForImagesIsSupportedThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isBlitterForImagesSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPageFaultIsSupportedThenReturnFalse) {

    EXPECT_FALSE(productHelper->isPageFaultSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfKmdMigrationIsSupportedThenReturnFalse) {

    EXPECT_FALSE(productHelper->isKmdMigrationSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfDisableScratchPagesIsSupportedThenReturnFalse, IsAtMostXeHpgCore) {
    EXPECT_FALSE(productHelper->isDisableScratchPagesSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfDisableScratchPagesIsSupportedForDebuggerThenReturnTrue, IsNotDG2) {
    EXPECT_TRUE(productHelper->isDisableScratchPagesRequiredForDebugger());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfPlatformSupportsSvmHeapReservationThenReturnTrue, IsXeHpcCore) {
    EXPECT_TRUE(productHelper->isSvmHeapReservationSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckBlitEnqueuePreferredThenReturnTrue) {
    EXPECT_TRUE(productHelper->blitEnqueuePreferred(false));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned) {

    EXPECT_TRUE(productHelper->isTile64With3DSurfaceOnBCSSupported(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskedIfPatIndexProgrammingSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isVmBindPatIndexProgrammingSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfIsTimestampWaitSupportedForEventsThenFalseIsReturned, IsGen12LP) {

    EXPECT_FALSE(productHelper->isTimestampWaitSupportedForEvents());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCallGetCommandBuffersPreallocatedPerCommandQueueThenReturnCorrectValue, IsGen12LP) {
    EXPECT_EQ(0u, productHelper->getCommandBuffersPreallocatedPerCommandQueue());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCallGetInternalHeapsPreallocatedThenReturnCorrectValue, IsGen12LP) {
    EXPECT_EQ(productHelper->getInternalHeapsPreallocated(), 0u);

    DebugManagerStateRestore restorer;
    debugManager.flags.SetAmountOfInternalHeapsToPreallocate.set(3);
    EXPECT_EQ(productHelper->getInternalHeapsPreallocated(), 3u);
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedIfIsTlbFlushRequiredThenTrueIsReturned, IsAtMostDg2) {
    EXPECT_TRUE(productHelper->isTlbFlushRequired());
}

HWTEST2_F(ProductHelperTest, givenProductHelperAndForceTlbFlushNotSetWhenAskedIfIsTlbFlushRequiredThenFalseIsReturned, IsNotPVC) {
    DebugManagerStateRestore restore{};
    debugManager.flags.ForceTlbFlush.set(0);
    EXPECT_FALSE(productHelper->isTlbFlushRequired());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenAskedGetSharedSystemPatIndexThenReturnDefaultValue, IsPVC) {
    EXPECT_EQ(0ull, productHelper->getSharedSystemPatIndex());
}

HWTEST_F(ProductHelperTest, givenLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};

    pInHwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::bufferHostMemory);
    EXPECT_TRUE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::localMemory);

    auto expectedDefaultValue = (productHelper->getLocalMemoryAccessMode(pInHwInfo) == LocalMemoryAccessMode::cpuAccessDisallowed);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];

    EXPECT_EQ(expectedDefaultValue, productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    debugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    debugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    pInHwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::system64KBPages);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    pInHwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
}

HWTEST_F(ProductHelperTest, givenNotLockableAllocationWhenGettingIsBlitCopyRequiredForLocalMemoryThenCorrectValuesAreReturned) {
    DebugManagerStateRestore restore{};
    HardwareInfo hwInfo = pInHwInfo;

    hwInfo.capabilityTable.blitterOperationsSupported = true;

    MockGraphicsAllocation graphicsAllocation;
    graphicsAllocation.setAllocationType(AllocationType::svmGpu);
    EXPECT_FALSE(GraphicsAllocation::isLockable(graphicsAllocation.getAllocationType()));
    graphicsAllocation.overrideMemoryPool(MemoryPool::localMemory);

    MockExecutionEnvironment executionEnvironment(&hwInfo);
    executionEnvironment.initGmm();
    executionEnvironment.prepareRootDeviceEnvironments(1);
    auto gmmHelper = executionEnvironment.rootDeviceEnvironments[0]->getGmmHelper();
    GmmRequirements gmmRequirements{};
    gmmRequirements.allowLargePages = true;
    gmmRequirements.preferCompressed = false;
    MockGmm mockGmm(gmmHelper, nullptr, 100, 100, GMM_RESOURCE_USAGE_OCL_BUFFER, {}, gmmRequirements);
    mockGmm.resourceParams.Flags.Info.NotLockable = true;
    graphicsAllocation.setDefaultGmm(&mockGmm);

    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];

    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    debugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    debugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = false;
    EXPECT_TRUE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));

    graphicsAllocation.overrideMemoryPool(MemoryPool::system64KBPages);
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
    hwInfo.capabilityTable.blitterOperationsSupported = true;
    EXPECT_FALSE(productHelper->isBlitCopyRequiredForLocalMemory(rootDeviceEnvironment, graphicsAllocation));
}

HWTEST_F(ProductHelperTest, givenSamplerStateWhenAdjustSamplerStateThenNothingIsChanged) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    auto state = FamilyType::cmdInitSamplerState;
    auto initialState = state;
    productHelper->adjustSamplerState(&state, pInHwInfo);

    EXPECT_EQ(0, memcmp(&initialState, &state, sizeof(SAMPLER_STATE)));
}

HWTEST2_F(ProductHelperTest, whenGettingNumberOfCacheRegionsThenReturnZero, IsClosUnsupported) {
    EXPECT_EQ(0u, productHelper->getNumCacheRegions());
}

HWTEST2_F(ProductHelperTest, whenGettingNumberOfCacheRegionsThenReturnNonZero, IsClosSupported) {
    EXPECT_NE(0u, productHelper->getNumCacheRegions());
}

HWTEST2_F(ProductHelperTest, WhenFillingScmPropertiesSupportThenExpectUseCorrectGetters, MatchAny) {
    StateComputeModePropertiesSupport scmPropertiesSupport = {};

    productHelper->fillScmPropertiesSupportStructure(scmPropertiesSupport);

    EXPECT_EQ(productHelper->isThreadArbitrationPolicyReportedWithScm(), scmPropertiesSupport.threadArbitrationPolicy);
    EXPECT_EQ(productHelper->getScmPropertyCoherencyRequiredSupport(), scmPropertiesSupport.coherencyRequired);
    EXPECT_EQ(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport(), scmPropertiesSupport.zPassAsyncComputeThreadLimit);
    EXPECT_EQ(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport(), scmPropertiesSupport.pixelAsyncComputeThreadLimit);
    EXPECT_EQ(productHelper->getScmPropertyDevicePreemptionModeSupport(), scmPropertiesSupport.devicePreemptionMode);
    if constexpr (TestTraits<FamilyType::gfxCoreFamily>::largeGrfModeInStateComputeModeSupported) {
        EXPECT_EQ(productHelper->isGrfNumReportedWithScm(), scmPropertiesSupport.largeGrfMode);
    }
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
    EXPECT_EQ(productHelper->isSystolicModeConfigurable(pInHwInfo), pipelineSelectPropertiesSupport.systolicMode);
}

HWTEST_F(ProductHelperTest, WhenFillingStateBaseAddressPropertiesSupportThenExpectUseCorrectGetters) {
    StateBaseAddressPropertiesSupport stateBaseAddressPropertiesSupport = {};

    productHelper->fillStateBaseAddressPropertiesSupportStructure(stateBaseAddressPropertiesSupport);
    EXPECT_EQ(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport(), stateBaseAddressPropertiesSupport.bindingTablePoolBaseAddress);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenisReleaseGlobalFenceInCommandStreamRequiredThenFalseIsReturned) {

    EXPECT_FALSE(productHelper->isReleaseGlobalFenceInCommandStreamRequired(*defaultHwInfo));
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

    debugManager.flags.ForceGrfNumProgrammingWithScm.set(0);
    EXPECT_FALSE(productHelper->isGrfNumReportedWithScm());

    debugManager.flags.ForceGrfNumProgrammingWithScm.set(1);
    EXPECT_TRUE(productHelper->isGrfNumReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenDefaultSettingWhenIsThreadArbitrationPolicyReportedIsCalledThenScmSupportProductValueReturned) {

    EXPECT_EQ(productHelper->getScmPropertyThreadArbitrationPolicySupport(), productHelper->isThreadArbitrationPolicyReportedWithScm());
}

HWTEST2_F(ProductHelperTest, givenForceThreadArbitrationPolicyProgrammingWithScmFlagSetWhenIsThreadArbitrationPolicyReportedWithScmIsQueriedThenCorrectValueIsReturned, IsAtMostXe3Core) {
    DebugManagerStateRestore restorer;

    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(0);
    EXPECT_FALSE(productHelper->isThreadArbitrationPolicyReportedWithScm());

    debugManager.flags.ForceThreadArbitrationPolicyProgrammingWithScm.set(1);
    EXPECT_TRUE(productHelper->isThreadArbitrationPolicyReportedWithScm());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsAdjustWalkOrderAvailableCallThenFalseReturn) {

    EXPECT_FALSE(productHelper->isAdjustWalkOrderAvailable(releaseHelper));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsPrefetcherDisablingInDirectSubmissionRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isPrefetcherDisablingInDirectSubmissionRequired());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsImplicitScalingSupportedThenExpectFalse, IsNotXeHpcCore) {
    EXPECT_FALSE(productHelper->isImplicitScalingSupported(*defaultHwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperAndDebugFlagWhenGetL1CachePolicyThenReturnCorrectPolicy, IsAtLeastXeCore) {
    DebugManagerStateRestore restorer;

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(0);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, productHelper->getL1CachePolicy(true));

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WB, productHelper->getL1CachePolicy(true));

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(3);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WT, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WT, productHelper->getL1CachePolicy(true));

    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(4);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WS, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WS, productHelper->getL1CachePolicy(true));

    debugManager.flags.ForceAllResourcesUncached.set(true);
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_UC, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenGetL1CachePolicyThenReturnWriteByPass, IsAtLeastXeCore) {
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenPlatformWithUnsupportedL1CachePoliciesWhenGetL1CachePolicyThenReturnZero, IsGen12LP) {
    EXPECT_EQ(0u, productHelper->getL1CachePolicy(false));
    EXPECT_EQ(0u, productHelper->getL1CachePolicy(true));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsStatefulAddressingModeSupportedThenReturnTrue, HasStatefulSupport) {
    EXPECT_TRUE(productHelper->isStatefulAddressingModeSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsPlatformQueryNotSupportedThenReturnFalse, IsAtMostDg2) {
    EXPECT_FALSE(productHelper->isPlatformQuerySupported());
}

HWTEST2_F(ProductHelperTest, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned, IsGen12LP) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));
}

HWTEST2_F(ProductHelperTest, givenDebugFlagWhenCheckingIsResolveDependenciesByPipeControlsSupportedThenCorrectValueIsReturned, IsXeHpgCore) {
    DebugManagerStateRestore restorer;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    MockCommandStreamReceiver csr(*mockDevice->getExecutionEnvironment(), mockDevice->getRootDeviceIndex(), mockDevice->getDeviceBitfield());
    csr.taskCount = 2;

    // ResolveDependenciesViaPipeControls = -1 (default)
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(0);
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_FALSE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));

    debugManager.flags.ResolveDependenciesViaPipeControls.set(1);
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 2, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, false, 3, csr));
    EXPECT_TRUE(productHelper->isResolveDependenciesByPipeControlsSupported(pInHwInfo, true, 3, csr));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned, IsGen12LP) {
    EXPECT_FALSE(productHelper->isBufferPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned, IsXeHpcCore) {
    EXPECT_FALSE(productHelper->isBufferPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsBufferPoolAllocatorSupportedThenCorrectValueIsReturned, IsXeHpgCore) {
    EXPECT_TRUE(productHelper->isBufferPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmPoolAllocatorSupportedThenCorrectValueIsReturned, IsGen12LP) {
    EXPECT_FALSE(productHelper->isDeviceUsmPoolAllocatorSupported());
    EXPECT_FALSE(productHelper->isHostUsmPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmPoolAllocatorSupportedThenCorrectValueIsReturned, IsXeHpcCore) {
    EXPECT_FALSE(productHelper->isDeviceUsmPoolAllocatorSupported());
    EXPECT_FALSE(productHelper->isHostUsmPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmPoolAllocatorSupportedThenCorrectValueIsReturned, IsXeHpgCore) {
    EXPECT_TRUE(productHelper->isDeviceUsmPoolAllocatorSupported());
    EXPECT_TRUE(productHelper->isHostUsmPoolAllocatorSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmAllocationReuseSupportedThenCorrectValueIsReturned, IsGen12LP) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_FALSE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmAllocationReuseSupportedThenCorrectValueIsReturned, IsXeHpcCore) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_FALSE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_FALSE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_FALSE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCheckingIsUsmAllocationReuseSupportedThenCorrectValueIsReturned, IsXe2HpgCore) {
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::OCL);
        EXPECT_TRUE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
    {
        VariableBackup<ApiSpecificConfig::ApiType> backup(&apiTypeForUlts, ApiSpecificConfig::L0);
        EXPECT_TRUE(productHelper->isHostUsmAllocationReuseSupported());
        EXPECT_TRUE(productHelper->isDeviceUsmAllocationReuseSupported());
    }
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIsUnlockingLockedPtrNecessaryThenReturnFalse) {
    EXPECT_FALSE(productHelper->isUnlockingLockedPtrNecessary(pInHwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperAndKernelBinaryFormatsWhenCheckingIsDetectIndirectAccessInKernelSupportedThenCorrectValueIsReturned) {
    KernelDescriptor kernelDescriptor;
    kernelDescriptor.kernelMetadata.kernelName = "long_kernel_name";
    auto kernelNamePart = "kernel";
    auto kernelNameList = "long_kernel_name;different_kernel_name";
    const auto igcDetectIndirectVersion = INDIRECT_ACCESS_DETECTION_VERSION;
    {
        const auto minimalRequiredDetectIndirectVersion = productHelper->getRequiredDetectIndirectVersion();
        EXPECT_GT(minimalRequiredDetectIndirectVersion, 0u);
        const bool detectionEnabled = IndirectDetectionVersions::disabled != minimalRequiredDetectIndirectVersion;
        if (detectionEnabled) {
            const uint32_t notAcceptedIndirectDetectionVersion = minimalRequiredDetectIndirectVersion - 1;
            const bool enabledForJit = igcDetectIndirectVersion >= minimalRequiredDetectIndirectVersion;
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
                kernelDescriptor.kernelAttributes.simdSize = 8u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));
            }
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::zebin;
                kernelDescriptor.kernelAttributes.simdSize = 8u;
                EXPECT_EQ(enabledForJit, productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersion));
                EXPECT_EQ(enabledForJit, productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));
                {
                    DebugManagerStateRestore restorer;
                    debugManager.flags.DisableIndirectDetectionForKernelNames.set(kernelNamePart);
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));

                    debugManager.flags.DisableIndirectDetectionForKernelNames.set(kernelNameList);
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));
                }
            }
        } else { // detection disabled
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
                kernelDescriptor.kernelAttributes.simdSize = 8u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));
            }
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::zebin;
                kernelDescriptor.kernelAttributes.simdSize = 8u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersion));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersion));
            }
        }
    }

    {
        const auto minimalRequiredDetectIndirectVersionVC = productHelper->getRequiredDetectIndirectVersionVC();
        EXPECT_GT(minimalRequiredDetectIndirectVersionVC, 0u);
        const bool detectionEnabledVC = IndirectDetectionVersions::disabled != minimalRequiredDetectIndirectVersionVC;
        if (detectionEnabledVC) {
            const uint32_t notAcceptedIndirectDetectionVersionVC = minimalRequiredDetectIndirectVersionVC - 1;
            const bool enabledForJitVC = igcDetectIndirectVersion >= minimalRequiredDetectIndirectVersionVC;
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
                kernelDescriptor.kernelAttributes.simdSize = 1u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
            }
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::zebin;
                kernelDescriptor.kernelAttributes.simdSize = 1u;
                EXPECT_EQ(enabledForJitVC, productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                EXPECT_EQ(enabledForJitVC, productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                {
                    DebugManagerStateRestore restorer;
                    debugManager.flags.DisableIndirectDetectionForKernelNames.set(kernelNamePart);
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));

                    debugManager.flags.DisableIndirectDetectionForKernelNames.set(kernelNameList);
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                }
                {
                    DebugManagerStateRestore restorer;
                    debugManager.flags.ForceIndirectDetectionForCMKernels.set(0);
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                    EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                }
                {
                    DebugManagerStateRestore restorer;
                    debugManager.flags.ForceIndirectDetectionForCMKernels.set(1);
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, notAcceptedIndirectDetectionVersionVC));
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                }
            }
        } else { // detection disabled for VC
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::patchtokens;
                kernelDescriptor.kernelAttributes.simdSize = 1u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
            }
            {
                kernelDescriptor.kernelAttributes.binaryFormat = DeviceBinaryFormat::zebin;
                kernelDescriptor.kernelAttributes.simdSize = 1u;
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                EXPECT_FALSE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                {
                    DebugManagerStateRestore restorer;
                    debugManager.flags.ForceIndirectDetectionForCMKernels.set(1);
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, false, minimalRequiredDetectIndirectVersionVC));
                    EXPECT_TRUE(productHelper->isDetectIndirectAccessInKernelSupported(kernelDescriptor, true, minimalRequiredDetectIndirectVersionVC));
                }
            }
        }
    }
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIsTranslationExceptionSupportedThenReturnFalse) {
    EXPECT_FALSE(productHelper->isTranslationExceptionSupported());
}

HWTEST_F(ProductHelperTest, whenQueryingMaxNumSamplersThenReturnSixteen) {
    EXPECT_EQ(16u, productHelper->getMaxNumSamplers());
}

HWTEST_F(ProductHelperTest, whenDisableL3ForDebugCalledThenFalseIsReturned) {
    EXPECT_FALSE(productHelper->disableL3CacheForDebug(*defaultHwInfo));
}

HWTEST_F(ProductHelperTest, givenBooleanUncachedWhenCallOverridePatIndexThenProperPatIndexIsReturned) {
    uint64_t patIndex = 1u;
    bool isUncached = true;
    EXPECT_EQ(patIndex, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));

    isUncached = false;
    EXPECT_EQ(patIndex, productHelper->overridePatIndex(isUncached, patIndex, AllocationType::buffer));
}

HWTEST_F(ProductHelperTest, givenGmmUsageTypeWhenCallingGetGmmResourceUsageOverrideThenReturnNoOverride) {
    constexpr uint32_t noOverride = GMM_RESOURCE_USAGE_UNKNOWN;
    EXPECT_EQ(noOverride, productHelper->getGmmResourceUsageOverride(GMM_RESOURCE_USAGE_OCL_BUFFER));
    EXPECT_EQ(noOverride, productHelper->getGmmResourceUsageOverride(GMM_RESOURCE_USAGE_XADAPTER_SHARED_RESOURCE));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingSupportedNumGrfsThenCorrectValueIsReturned) {
    if (releaseHelper) {
        EXPECT_EQ(releaseHelper->getSupportedNumGrfs(), productHelper->getSupportedNumGrfs(releaseHelper));
    } else {
        const std::vector<uint32_t> expectedValues{128u};
        EXPECT_EQ(expectedValues, productHelper->getSupportedNumGrfs(releaseHelper));
    }
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingDefaultCopyEngineThenEngineBCSIsReturned) {
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, productHelper->getDefaultCopyEngine());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAdjustingEnginesGroupThenDoNotChangeEngineGroups) {
    for (uint32_t engineGroupIt = static_cast<uint32_t>(EngineGroupType::compute); engineGroupIt < static_cast<uint32_t>(EngineGroupType::maxEngineGroups); engineGroupIt++) {
        auto engineGroupType = static_cast<EngineGroupType>(engineGroupIt);
        auto engineGroupTypeUnchanged = engineGroupType;
        productHelper->adjustEngineGroupType(engineGroupType);
        EXPECT_EQ(engineGroupTypeUnchanged, engineGroupType);
    }
}

HWTEST_F(ProductHelperTest, whenGettingPreferredAllocationMethodThenNoPreferenceIsReturned) {
    for (auto i = 0; i < static_cast<int>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        auto preferredAllocationMethod = productHelper->getPreferredAllocationMethod(allocationType);
        EXPECT_FALSE(preferredAllocationMethod.has_value());
    }
}

HWTEST_F(ProductHelperTest, whenAskingForLocalDispatchSizeThenReturnEmpty) {
    EXPECT_EQ(0u, productHelper->getSupportedLocalDispatchSizes(pInHwInfo).size());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskingForReadOnlyResourceSupportThenFalseReturned) {
    EXPECT_FALSE(productHelper->supportReadOnlyAllocations());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskingForSharingWith3dOrMediaSupportThenTrueReturned) {
    EXPECT_TRUE(productHelper->isSharingWith3dOrMediaAllowed());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskingForDeviceToHostCopySignalingFenceFalseReturned) {
    EXPECT_FALSE(productHelper->isDeviceToHostCopySignalingFenceRequired());
}

HWTEST2_F(ProductHelperTest, givenPatIndexWhenCheckIsCoherentAllocationThenReturnNullopt, IsAtMostPVC) {
    std::array<uint64_t, 5> listOfCoherentPatIndexes = {0, 1, 2, 3, 4};
    for (auto patIndex : listOfCoherentPatIndexes) {
        EXPECT_EQ(std::nullopt, productHelper->isCoherentAllocation(patIndex));
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenItsPreXe2ThenCacheLineSizeIs64Bytes, IsAtMostPVC) {
    EXPECT_EQ(productHelper->getCacheLineSize(), 64u);
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenItsXe2PlusThenCacheLineSizeIs256Bytes, IsAtLeastXe2HpgCore) {
    EXPECT_EQ(productHelper->getCacheLineSize(), 256u);
}

TEST_F(ProductHelperTest, whenGettingMaxSubSliceSpaceThenValueIsNotSmallerThanMaxSubSliceCount) {
    constexpr auto maxSupportedSubSlices = 128u;
    auto hwInfo = *defaultHwInfo;
    auto &gtSystemInfo = hwInfo.gtSystemInfo;
    gtSystemInfo.SliceCount = 1;
    gtSystemInfo.SubSliceCount = 2;
    gtSystemInfo.DualSubSliceCount = 2;

    gtSystemInfo.MaxSlicesSupported = 2;
    gtSystemInfo.MaxSlicesSupported = 2;
    gtSystemInfo.MaxSubSlicesSupported = maxSupportedSubSlices;
    gtSystemInfo.MaxDualSubSlicesSupported = maxSupportedSubSlices;

    gtSystemInfo.IsDynamicallyPopulated = true;
    for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo.SliceInfo[slice].Enabled = slice < gtSystemInfo.SliceCount;
    }
    EXPECT_EQ(maxSupportedSubSlices, productHelper->computeMaxNeededSubSliceSpace(hwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenQuery2DBlockLoadThenReturnFalse, IsAtMostXeHpgCore) {

    EXPECT_FALSE(productHelper->supports2DBlockLoad());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenQuery2DBlockStoreThenReturnFalse, IsAtMostXeHpgCore) {

    EXPECT_FALSE(productHelper->supports2DBlockStore());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenQuery2DBlockLoadThenReturnTrue, IsWithinXeHpcCoreAndXe3Core) {

    EXPECT_TRUE(productHelper->supports2DBlockLoad());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenQuery2DBlockStoreThenReturnTrue, IsWithinXeHpcCoreAndXe3Core) {

    EXPECT_TRUE(productHelper->supports2DBlockStore());
}

HWTEST2_F(ProductHelperTest, WhenGetSvmCpuAlignmentThenProperValueIsReturned, IsAtLeastXeHpcCore) {
    EXPECT_EQ(MemoryConstants::pageSize64k, productHelper->getSvmCpuAlignment());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenGetRequiredDetectIndirectVersionCalledThenReturnCorrectVersion, IsNotPVC) {
    EXPECT_EQ(9u, productHelper->getRequiredDetectIndirectVersion());
    EXPECT_EQ(6u, productHelper->getRequiredDetectIndirectVersionVC());
}

HWTEST_F(ProductHelperTest, whenAdjustScratchSizeThenSizeIsNotChanged) {
    constexpr size_t initialScratchSize = 0xDEADBEEF;
    size_t scratchSize = initialScratchSize;
    productHelper->adjustScratchSize(scratchSize);
    EXPECT_EQ(initialScratchSize, scratchSize);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIs2MBLocalMemAlignmentEnabledThenCorrectValueIsReturned) {
    EXPECT_FALSE(productHelper->is2MBLocalMemAlignmentEnabled());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenGetMaxLocalSubRegionSizeCalledThenZeroIsReturned, IsAtMostXe3Core) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_EQ(0u, productHelper->getMaxLocalSubRegionSize(hwInfo));
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCheckingIsCompressionForbiddenThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;
    auto hwInfo = *defaultHwInfo;

    debugManager.flags.RenderCompressedImagesEnabled.set(0);
    debugManager.flags.RenderCompressedBuffersEnabled.set(0);
    EXPECT_TRUE(productHelper->isCompressionForbidden(hwInfo));

    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));

    debugManager.flags.RenderCompressedImagesEnabled.set(0);
    debugManager.flags.RenderCompressedBuffersEnabled.set(1);
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));

    debugManager.flags.RenderCompressedImagesEnabled.set(1);
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperThenCompressionIsNotForbidden, IsAtLeastXe2HpgCore) {
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(productHelper->isCompressionForbidden(hwInfo));
}

HWTEST2_F(ProductHelperTest, givenProductHelperBeforeXe2WhenOverrideDirectSubmissionTimeoutsThenTimeoutsNotAdjusted, IsAtMostXeCore) {
    DebugManagerStateRestore restorer;

    uint64_t timeoutUs{5000};
    uint64_t maxTimeoutUs{5000};
    productHelper->overrideDirectSubmissionTimeouts(timeoutUs, maxTimeoutUs);
    EXPECT_EQ(5000ull, timeoutUs);
    EXPECT_EQ(5000ull, maxTimeoutUs);
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenOverrideDirectSubmissionTimeoutsThenTimeoutsAdjusted, IsAtLeastXe2HpgCore) {
    DebugManagerStateRestore restorer;

    uint64_t timeoutUs{5000};
    uint64_t maxTimeoutUs{5000};
    productHelper->overrideDirectSubmissionTimeouts(timeoutUs, maxTimeoutUs);
    EXPECT_EQ(1000ull, timeoutUs);
    EXPECT_EQ(1000ull, maxTimeoutUs);

    debugManager.flags.DirectSubmissionControllerTimeout.set(10000);
    debugManager.flags.DirectSubmissionControllerMaxTimeout.set(10000);
    productHelper->overrideDirectSubmissionTimeouts(timeoutUs, maxTimeoutUs);
    EXPECT_EQ(10000ull, timeoutUs);
    EXPECT_EQ(10000ull, maxTimeoutUs);
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenIsExposingSubdevicesAllowedThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->isExposingSubdevicesAllowed());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingIsPrimaryContextsAggregationSupportedThenReturnCorrectValue) {
    EXPECT_FALSE(productHelper->isPrimaryContextsAggregationSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenCallingUseAdditionalBlitPropertiesThenFalseReturned) {
    EXPECT_FALSE(productHelper->useAdditionalBlitProperties());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCallingIsResourceUncachedForCSThenFalseReturned, IsAtMostXeCore) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        EXPECT_FALSE(productHelper->isResourceUncachedForCS(allocationType));
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenCallingIsResourceUncachedForCSThenTrueReturned, IsAtLeastXe2HpgCore) {
    for (uint32_t i = 0; i < static_cast<uint32_t>(AllocationType::count); i++) {
        auto allocationType = static_cast<AllocationType>(i);
        if (allocationType == AllocationType::commandBuffer ||
            allocationType == AllocationType::ringBuffer ||
            allocationType == AllocationType::semaphoreBuffer) {
            EXPECT_TRUE(productHelper->isResourceUncachedForCS(allocationType));
        } else {
            EXPECT_FALSE(productHelper->isResourceUncachedForCS(allocationType));
        }
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenisPackedCopyFormatSupportedThenCorrectValueIsReturned, IsAtMostXe3Core) {
    EXPECT_FALSE(productHelper->isPackedCopyFormatSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenGettingPreferredWorkgroupCountPerSubsliceThenZeroReturned) {
    EXPECT_EQ(0u, productHelper->getPreferredWorkgroupCountPerSubslice());
}

HWTEST_F(ProductHelperTest, givenProductHelperWithDebugKeyWhenPidFdOrSocketForIpcIsSupportedThenExpectedValueReturned) {
    DebugManagerStateRestore restore;

    debugManager.flags.EnablePidFdOrSocketsForIpc.set(1);
    EXPECT_TRUE(productHelper->isPidFdOrSocketForIpcSupported());

    debugManager.flags.EnablePidFdOrSocketsForIpc.set(0);
    EXPECT_FALSE(productHelper->isPidFdOrSocketForIpcSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenPidFdOrSocketForIpcIsNotSupportedThenFalseReturned, IsAtLeastXe2HpgCore) {
    EXPECT_FALSE(productHelper->isPidFdOrSocketForIpcSupported());
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenPidFdOrSocketForIpcIsNotSupportedThenFalseReturned, IsAtMostXeCore) {
    EXPECT_FALSE(productHelper->isPidFdOrSocketForIpcSupported());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskingShouldRegisterEnqueuedWalkerWithProfilingThenFalseReturned) {
    EXPECT_FALSE(productHelper->shouldRegisterEnqueuedWalkerWithProfiling());
}

HWTEST_F(ProductHelperTest, givenProductHelperWhenAskingIsInterruptSupportedThenFalseReturned) {
    EXPECT_FALSE(productHelper->isInterruptSupported());
}

HWTEST2_F(ProductHelperTest, givenDG1ProductHelperWhenCanShareMemoryWithoutNTHandleIsCalledThenFalseIsReturned, IsDG1) {
    EXPECT_FALSE(productHelper->canShareMemoryWithoutNTHandle());
}

HWTEST2_F(ProductHelperTest, givenNonDG1ProductHelperWhenCanShareMemoryWithoutNTHandleIsCalledThenTrueIsReturned, IsNotDG1) {
    EXPECT_TRUE(productHelper->canShareMemoryWithoutNTHandle());
}
