/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/os_interface/hw_info_config_tests.h"

#include "shared/source/helpers/driver_model_type.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include "gtest/gtest.h"

using namespace NEO;

void HwInfoConfigTest::SetUp() {
    PlatformFixture::SetUp();

    pInHwInfo = pPlatform->getClDevice(0)->getHardwareInfo();

    testPlatform = &pInHwInfo.platform;
    testSkuTable = &pInHwInfo.featureTable;
    testWaTable = &pInHwInfo.workaroundTable;
    testSysInfo = &pInHwInfo.gtSystemInfo;

    outHwInfo = {};
}

void HwInfoConfigTest::TearDown() {
    PlatformFixture::TearDown();
}

HWTEST_F(HwInfoConfigTest, givenDebugFlagSetWhenAskingForHostMemCapabilitesThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);

    DebugManager.flags.EnableHostUsmSupport.set(0);
    EXPECT_EQ(0u, hwInfoConfig->getHostMemCapabilities(&pInHwInfo));

    DebugManager.flags.EnableHostUsmSupport.set(1);
    EXPECT_NE(0u, hwInfoConfig->getHostMemCapabilities(&pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenGettingSharedSystemMemCapabilitiesThenCorrectValueIsReturned) {
    DebugManagerStateRestore restore;

    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_EQ(0u, hwInfoConfig->getSharedSystemMemCapabilities(&pInHwInfo));

    for (auto enable : {-1, 0, 1}) {
        DebugManager.flags.EnableSharedSystemUsmSupport.set(enable);

        if (enable > 0) {
            auto caps = UNIFIED_SHARED_MEMORY_ACCESS | UNIFIED_SHARED_MEMORY_ATOMIC_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ACCESS | UNIFIED_SHARED_MEMORY_CONCURRENT_ATOMIC_ACCESS;
            EXPECT_EQ(caps, hwInfoConfig->getSharedSystemMemCapabilities(&pInHwInfo));
        } else {
            EXPECT_EQ(0u, hwInfoConfig->getSharedSystemMemCapabilities(&pInHwInfo));
        }
    }
}

TEST_F(HwInfoConfigTest, WhenParsingHwInfoConfigThenCorrectValuesAreReturned) {
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

TEST_F(HwInfoConfigTest, givenInvalidHwInfoWhenParsingHwInfoConfigThenErrorIsReturned) {
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

HWTEST_F(HwInfoConfigTest, whenConvertingTimestampsToCsDomainThenNothingIsChanged) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    uint64_t timestampData = 0x1234;
    uint64_t initialData = timestampData;
    hwInfoConfig->convertTimestampsFromOaToCsDomain(timestampData);
    EXPECT_EQ(initialData, timestampData);
}

HWTEST_F(HwInfoConfigTest, whenOverrideGfxPartitionLayoutForWslThenReturnFalse) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig->overrideGfxPartitionLayoutForWsl());
}

HWTEST_F(HwInfoConfigTest, givenSamplerStateWhenAdjustSamplerStateThenNothingIsChanged) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    auto context = clUniquePtr(new MockContext());
    auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
    auto state = FamilyType::cmdInitSamplerState;
    auto initialState = state;
    hwInfoConfig->adjustSamplerState(&state, pInHwInfo);

    EXPECT_EQ(0, memcmp(&initialState, &state, sizeof(SAMPLER_STATE)));
}

HWTEST_F(HwInfoConfigTest, givenHardwareInfoWhenCallingIsAdditionalStateBaseAddressWARequiredThenFalseIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    bool ret = hwInfoConfig->isAdditionalStateBaseAddressWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}

HWTEST_F(HwInfoConfigTest, givenHardwareInfoWhenCallingIsMaxThreadsForWorkgroupWARequiredThenFalseIsReturned) {
    auto hwInfoConfig = HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    bool ret = hwInfoConfig->isMaxThreadsForWorkgroupWARequired(pInHwInfo);

    EXPECT_FALSE(ret);
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedForPageTableManagerSupportThenReturnCorrectValue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.isPageTableManagerSupported(pInHwInfo), UnitTestHelper<FamilyType>::isPageTableManagerSupported(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenVariousValuesWhenConvertingHwRevIdAndSteppingThenConversionIsCorrect) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);

    for (uint32_t testValue = 0; testValue < 0x10; testValue++) {
        auto hwRevIdFromStepping = hwInfoConfig.getHwRevIdFromStepping(testValue, pInHwInfo);
        if (hwRevIdFromStepping != CommonConstants::invalidStepping) {
            pInHwInfo.platform.usRevId = hwRevIdFromStepping;
            EXPECT_EQ(testValue, hwInfoConfig.getSteppingFromHwRevId(pInHwInfo));
        }
        pInHwInfo.platform.usRevId = testValue;
        auto steppingFromHwRevId = hwInfoConfig.getSteppingFromHwRevId(pInHwInfo);
        if (steppingFromHwRevId != CommonConstants::invalidStepping) {
            EXPECT_EQ(testValue, hwInfoConfig.getHwRevIdFromStepping(steppingFromHwRevId, pInHwInfo));
        }
    }
}

HWTEST_F(HwInfoConfigTest, givenVariousValuesWhenGettingAubStreamSteppingFromHwRevIdThenReturnValuesAreCorrect) {
    struct MockHwInfoConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
        uint32_t getSteppingFromHwRevId(const HardwareInfo &hwInfo) const override {
            return returnedStepping;
        }
        std::vector<int32_t> getKernelSupportedThreadArbitrationPolicies() override {
            return {};
        }
        uint32_t returnedStepping = 0;
    };
    MockHwInfoConfig mockHwInfoConfig;
    mockHwInfoConfig.returnedStepping = REVISION_A0;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_A1;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_A3;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_B;
    EXPECT_EQ(AubMemDump::SteppingValues::B, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_C;
    EXPECT_EQ(AubMemDump::SteppingValues::C, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_D;
    EXPECT_EQ(AubMemDump::SteppingValues::D, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = REVISION_K;
    EXPECT_EQ(AubMemDump::SteppingValues::K, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
    mockHwInfoConfig.returnedStepping = CommonConstants::invalidStepping;
    EXPECT_EQ(AubMemDump::SteppingValues::A, mockHwInfoConfig.getAubStreamSteppingFromHwRevId(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedForDefaultEngineTypeAdjustmentThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isDefaultEngineTypeAdjustmentRequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, whenCallingGetDeviceMemoryNameThenDdrIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    auto deviceMemoryName = hwInfoConfig.getDeviceMemoryName();
    EXPECT_TRUE(hasSubstr(deviceMemoryName, std::string("DDR")));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HwInfoConfigTest, givenHwInfoConfigWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isDisableOverdispatchAvailable(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, WhenAllowRenderCompressionIsCalledThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.allowCompression(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, WhenAllowStatelessCompressionIsCalledThenReturnCorrectValue) {
    DebugManagerStateRestore restore;

    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(pInHwInfo));

    for (auto enable : {-1, 0, 1}) {
        DebugManager.flags.EnableStatelessCompression.set(enable);

        if (enable > 0) {
            EXPECT_TRUE(hwInfoConfig.allowStatelessCompression(pInHwInfo));
        } else {
            EXPECT_FALSE(hwInfoConfig.allowStatelessCompression(pInHwInfo));
        }
    }
}

HWTEST_F(HwInfoConfigTest, givenVariousDebugKeyValuesWhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    struct MockHwInfoConfig : HwInfoConfigHw<IGFX_UNKNOWN> {
        using HwInfoConfig::getDefaultLocalMemoryAccessMode;
    };

    DebugManagerStateRestore restore{};
    auto mockHwInfoConfig = static_cast<MockHwInfoConfig &>(*HwInfoConfig::get(productFamily));
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_EQ(mockHwInfoConfig.getDefaultLocalMemoryAccessMode(pInHwInfo), mockHwInfoConfig.getLocalMemoryAccessMode(pInHwInfo));

    DebugManager.flags.ForceLocalMemoryAccessMode.set(0);
    EXPECT_EQ(LocalMemoryAccessMode::Default, hwInfoConfig.getLocalMemoryAccessMode(pInHwInfo));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(1);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessAllowed, hwInfoConfig.getLocalMemoryAccessMode(pInHwInfo));
    DebugManager.flags.ForceLocalMemoryAccessMode.set(3);
    EXPECT_EQ(LocalMemoryAccessMode::CpuAccessDisallowed, hwInfoConfig.getLocalMemoryAccessMode(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfAllocationSizeAdjustmentIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAllocationSizeAdjustmentRequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfPrefetchDisablingIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isPrefetchDisablingRequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfPipeControlPriorToNonPipelinedStateCommandsWARequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    auto isRcs = false;
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(pInHwInfo, isRcs);

    EXPECT_FALSE(isExtendedWARequired);
    EXPECT_FALSE(isBasicWARequired);
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfHeapInLocalMemThenFalseIsReturned, IsAtMostGen12lp) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.heapInLocalMem(pInHwInfo));
}

HWTEST2_F(HwInfoConfigTest, givenHwInfoConfigWhenSettingCapabilityCoherencyFlagThenFlagIsSet, IsAtMostGen11) {
    auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);

    bool coherency = false;
    hwInfoConfig.setCapabilityCoherencyFlag(pInHwInfo, coherency);
    EXPECT_TRUE(coherency);
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfAdditionalMediaSamplerProgrammingIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isAdditionalMediaSamplerProgrammingRequired());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfInitialFlagsProgrammingIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isInitialFlagsProgrammingRequired());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfReturnedCmdSizeForMediaSamplerAdjustmentIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isReturnedCmdSizeForMediaSamplerAdjustmentRequired());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfExtraParametersAreInvalidThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.extraParametersInvalid(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfPipeControlWAIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.pipeControlWARequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfImagePitchAlignmentWAIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.imagePitchAlignmentWARequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfForceEmuInt32DivRemSPWAIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isForceEmuInt32DivRemSPWARequired(pInHwInfo));
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.is3DPipelineSelectWARequired());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfStorageInfoAdjustmentIsRequiredThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_FALSE(hwInfoConfig.isStorageInfoAdjustmentRequired());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfBlitterForImagesIsSupportedThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_FALSE(hwInfoConfig.isBlitterForImagesSupported());
}

HWTEST_F(HwInfoConfigTest, givenHwInfoConfigWhenAskedIfTile64With3DSurfaceOnBCSIsSupportedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(pInHwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isTile64With3DSurfaceOnBCSSupported(pInHwInfo));
}
