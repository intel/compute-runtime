/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_aub_center.h"
#include "shared/test/common/mocks/mock_aub_manager.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

#include "aubstream/product_family.h"
#include "neo_aot_platforms.h"

using namespace NEO;

using TgllpHwInfo = ::testing::Test;

TGLLPTEST_F(TgllpHwInfo, whenSettingDeviceIdThenGeometryIsPickedBasedOnDeviceId) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    for (const auto &deviceId : tgllpDeviceIds) {
        hwInfo.platform.usDeviceID = deviceId;
        gtSystemInfo = {0};
        hardwareInfoSetup[productFamily](&hwInfo, false);
        EXPECT_EQ(1u, gtSystemInfo.SliceCount);
        if (TGLLP::isHw1x2x16(hwInfo)) {
            EXPECT_EQ(2u, gtSystemInfo.DualSubSliceCount);
            EXPECT_EQ_VAL(1920u, gtSystemInfo.L3CacheSizeInKb);
            EXPECT_EQ(4u, gtSystemInfo.L3BankCount);
        } else {
            EXPECT_EQ(6u, gtSystemInfo.DualSubSliceCount);
            EXPECT_EQ_VAL(3840u, gtSystemInfo.L3CacheSizeInKb);
            EXPECT_EQ(8u, gtSystemInfo.L3BankCount);
        }
    }
}

TGLLPTEST_F(TgllpHwInfo, givenBoolWhenCallTgllpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool);

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
        EXPECT_FALSE(featureTable.flags.ftrHeaplessMode);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.flags.waUntypedBufferCompression);
    }
}

TGLLPTEST_F(TgllpHwInfo, givenSetCommandStreamReceiverInAubModeForTgllpProductFamilyWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenAubCenterIsInitializedCorrectly) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.SetCommandStreamReceiver.set(1);
    debugManager.flags.ProductFamilyOverride.set("tgllp");

    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get());

    bool success = DeviceFactory::prepareDeviceEnvironmentsForProductFamilyOverride(executionEnvironment);
    ASSERT_TRUE(success);

    auto rootDeviceEnvironment = static_cast<MockRootDeviceEnvironment *>(executionEnvironment.rootDeviceEnvironments[0].get());

    EXPECT_TRUE(rootDeviceEnvironment->initAubCenterCalled);
    EXPECT_FALSE(rootDeviceEnvironment->localMemoryEnabledReceived);
}

TGLLPTEST_F(TgllpHwInfo, givenSetCommandStreamReceiverInAubModeWithOverrideGpuAddressSpaceWhenPrepareDeviceEnvironmentsForProductFamilyOverrideIsCalledThenAubManagerIsInitializedWithCorrectGpuAddressSpace) {
    DebugManagerStateRestore stateRestore;
    debugManager.flags.UseAubStream.set(1);
    debugManager.flags.SetCommandStreamReceiver.set(1);
    debugManager.flags.ProductFamilyOverride.set("tgllp");
    debugManager.flags.OverrideGpuAddressSpace.set(48);

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
    debugManager.flags.CreateMultipleRootDevices.set(requiredDeviceCount);
    debugManager.flags.SetCommandStreamReceiver.set(1);
    debugManager.flags.ProductFamilyOverride.set("tgllp");

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

using TgllpProductHelper = ProductHelperTest;

TGLLPTEST_F(TgllpProductHelper, whenGettingAubstreamProductFamilyThenProperEnumValueIsReturned) {
    EXPECT_EQ(aub_stream::ProductFamily::Tgllp, productHelper->getAubStreamProductFamily());
}

TGLLPTEST_F(TgllpProductHelper, givenProductHelperWhenGetCommandsStreamPropertiesSupportThenExpectCorrectValues) {

    EXPECT_FALSE(productHelper->getScmPropertyThreadArbitrationPolicySupport());
    EXPECT_TRUE(productHelper->getScmPropertyCoherencyRequiredSupport());
    EXPECT_FALSE(productHelper->getScmPropertyZPassAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyPixelAsyncComputeThreadLimitSupport());
    EXPECT_FALSE(productHelper->getScmPropertyLargeGrfModeSupport());
    EXPECT_FALSE(productHelper->getScmPropertyDevicePreemptionModeSupport());

    EXPECT_FALSE(productHelper->getStateBaseAddressPropertyBindingTablePoolBaseAddressSupport());

    EXPECT_TRUE(productHelper->getFrontEndPropertyScratchSizeSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyPrivateScratchSizeSupport());

    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyPreemptionModeSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyStateSipSupport());
    EXPECT_TRUE(productHelper->getPreemptionDbgPropertyCsrSurfaceSupport());

    EXPECT_FALSE(productHelper->getFrontEndPropertyComputeDispatchAllWalkerSupport());
    EXPECT_TRUE(productHelper->getFrontEndPropertyDisableEuFusionSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertyDisableOverDispatchSupport());
    EXPECT_FALSE(productHelper->getFrontEndPropertySingleSliceDispatchCcsModeSupport());

    EXPECT_FALSE(productHelper->getPipelineSelectPropertySystolicModeSupport());
}

TGLLPTEST_F(TgllpProductHelper, givenA0SteppingAndTgllpPlatformWhenAskingIfWAIsRequiredThenReturnTrue) {
    std::array<std::pair<uint32_t, bool>, 3> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, false},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = productHelper->getHwRevIdFromStepping(revision, hwInfo);
        productHelper->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, productHelper->pipeControlWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper->imagePitchAlignmentWARequired(hwInfo));
        EXPECT_EQ(paramBool, productHelper->isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

TGLLPTEST_F(TgllpProductHelper, givenProductHelperWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    EXPECT_TRUE(productHelper->is3DPipelineSelectWARequired());
}

TGLLPTEST_F(TgllpProductHelper, givenTgllpWhenObtainingBlitterPreferenceThenReturnFalse) {
    EXPECT_FALSE(productHelper->obtainBlitterPreference(pInHwInfo));
}

TGLLPTEST_F(TgllpProductHelper, givenCompilerProductHelperWhenGetProductConfigThenCorrectMatchIsFound) {
    EXPECT_EQ(compilerProductHelper->getHwIpVersion(pInHwInfo), AOT::TGL);
}
