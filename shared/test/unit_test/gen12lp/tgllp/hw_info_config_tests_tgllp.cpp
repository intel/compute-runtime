/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/hw_info_config_tests.h"

#include "aubstream/product_family.h"
#include "platforms.h"

using namespace NEO;

using TgllpHwInfo = HwInfoConfigTest;

TGLLPTEST_F(TgllpHwInfo, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Tgllp, productHelper->getAubStreamProductFamily());
}

using HwInfoConfigTestTgllp = Test<DeviceFixture>;

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenGettingEvictIfNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionIfNecessaryFlagSupported());
}

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    EXPECT_FALSE(hwInfoConfig.getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(hwInfoConfig.getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(hwInfoConfig.getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(hwInfoConfig.getStateBaseAddressPropertyGlobalAtomicsSupport());
    EXPECT_TRUE(hwInfoConfig.getStateBaseAddressPropertyStatelessMocsSupport());
    EXPECT_FALSE(hwInfoConfig.getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyScratchSizeSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(hwInfoConfig.getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(hwInfoConfig.getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(hwInfoConfig.getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyModeSelectedSupport());
    EXPECT_TRUE(hwInfoConfig.getPipelineSelectPropertyMediaSamplerDopClockGateSupport());
    EXPECT_FALSE(hwInfoConfig.getPipelineSelectPropertySystolicModeSupport());
}

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoErrorneousConfigStringThenThrow) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

TGLLPTEST_F(HwInfoConfigTestTgllp, whenUsingCorrectConfigValueThenCorrectHwInfoIsReturned) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0x100060010;

    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, config);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(6u, gtSystemInfo.DualSubSliceCount);

    config = 0x100020010;

    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, config);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(2u, gtSystemInfo.DualSubSliceCount);
}

TGLLPTEST_F(HwInfoConfigTestTgllp, givenA0SteppingAndTgllpPlatformWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto &productHelper = getHelper<ProductHelper>();
    std::array<std::pair<uint32_t, bool>, 3> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, false},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        productHelper.configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper.pipeControlWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper.imagePitchAlignmentWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper.isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

TGLLPTEST_F(HwInfoConfigTestTgllp, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.is3DPipelineSelectWARequired());
}

TGLLPTEST_F(TgllpHwInfo, givenBoolWhenCallTgllpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t configs[] = {
        0x100060010,
        0x100020010};

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);

            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waUntypedBufferCompression);
        }
    }
}

TGLLPTEST_F(TgllpHwInfo, givenHwInfoConfigStringThenAfterSetupResultingVmeIsDisabled) {
    HardwareInfo hwInfo = *defaultHwInfo;

    uint64_t config = 0x100060010;
    hardwareInfoSetup[productFamily](&hwInfo, false, config);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(hwInfo.capabilityTable.supportsVme);
}

TGLLPTEST_F(TgllpHwInfo, givenSetCommandStreamReceiverInAubModeForTgllpProductFamilyWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenAubCenterIsInitializedCorrectly) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.SetCommandStreamReceiver.set(1);
    DebugManager.flags.ProductFamilyOverride.set("tgllp");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_FALSE(rootDeviceEnvironment->localMemoryEnabledReceived);
}

TGLLPTEST_F(TgllpHwInfo, givenSetCommandStreamReceiverInAubModeWithOverrideGpuAddressSpaceWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenAubManagerIsInitializedWithCorrectGpuAddressSpace) {
    DebugManagerStateRestore stateRestore;
    DebugManager.flags.UseAubStream.set(1);
    DebugManager.flags.SetCommandStreamReceiver.set(1);
    DebugManager.flags.ProductFamilyOverride.set("tgllp");
    DebugManager.flags.OverrideGpuAddressSpace.set(48);

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    auto mockAubManager = static_cast<MockAubManager *>(rootDeviceEnvironment->aubCenter->getAubManager());
    EXPECT_EQ(MemoryConstants::max48BitAddress, mockAubManager->options.gpuAddressSpace);
}

TGLLPTEST_F(TgllpHwInfo, givenSetCommandStreamReceiverInAubModeWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenAllRootDeviceEnvironmentMembersAreInitialized) {
    DebugManagerStateRestore stateRestore;
    auto requiredDeviceCount = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);
    DebugManager.flags.SetCommandStreamReceiver.set(1);
    DebugManager.flags.ProductFamilyOverride.set("tgllp");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, requiredDeviceCount);

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);

    std::set<MemoryOperationsHandler *> memoryOperationHandlers;
    for (auto rootDeviceIndex = 0u; rootDeviceIndex < requiredDeviceCount; rootDeviceIndex++) {
        auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[rootDeviceIndex].get());
        EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
        EXPECT_FALSE(rootDeviceEnvironment->localMemoryEnabledReceived);

        auto memoryOperationInterface = rootDeviceEnvironment->memoryOperationsInterface.get();
        EXPECT_NE(nullptr, memoryOperationInterface);
        EXPECT_EQ(memoryOperationHandlers.end(), memoryOperationHandlers.find(memoryOperationInterface));
        memoryOperationHandlers.insert(memoryOperationInterface);
    }
}

TGLLPTEST_F(TgllpHwInfo, givenTgllpWhenObtainingBlitterPreferenceThenReturnFalse) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const auto &hardwareInfo = *defaultHwInfo;

    EXPECT_FALSE(hwInfoConfig.obtainBlitterPreference(hardwareInfo));
}

TGLLPTEST_F(TgllpHwInfo, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getProductConfigFromHwInfo(hwInfo), AOT::TGL);
}
