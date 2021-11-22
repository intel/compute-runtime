/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"

using namespace NEO;

using Dg2HwInfoConfig = ::testing::Test;

DG2TEST_F(Dg2HwInfoConfig, givenHwInfoErrorneousConfigStringThenThrow) {
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

DG2TEST_F(Dg2HwInfoConfig, givenDg2HwInfoConfigWhenCheckDirectSubmissionSupportedThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_TRUE(hwInfoConfig.isDirectSubmissionSupported(hwInfo));
}

DG2TEST_F(Dg2HwInfoConfig, givenHwInfoConfigWhenAdditionalKernelExecInfoSupportCheckedThenCorrectValueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto hwInfo = *defaultHwInfo;
    EXPECT_FALSE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));

    hwInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hwInfo);
    EXPECT_TRUE(hwInfoConfig.isDisableOverdispatchAvailable(hwInfo));
}

HWTEST_EXCLUDE_PRODUCT(HwHelperTestXeHPAndLater, GiveCcsNodeThenDefaultEngineTypeIsCcs, IGFX_DG2);

DG2TEST_F(Dg2HwInfoConfig, whenAdjustingDefaultEngineTypeThenSelectEngineTypeBasedOnRevisionId) {
    auto hardwareInfo = *defaultHwInfo;
    hardwareInfo.featureTable.ftrCCSNode = true;
    auto &hwHelper = HwHelper::get(renderCoreFamily);
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);

    hardwareInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_A0, hardwareInfo);
    hwHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_RCS, hardwareInfo.capabilityTable.defaultEngineType);

    hardwareInfo.capabilityTable.defaultEngineType = defaultHwInfo->capabilityTable.defaultEngineType;
    hardwareInfo.platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(REVISION_B, hardwareInfo);
    hwHelper.adjustDefaultEngineType(&hardwareInfo);
    EXPECT_EQ(aub_stream::ENGINE_CCS, hardwareInfo.capabilityTable.defaultEngineType);
}

DG2TEST_F(Dg2HwInfoConfig, givenA0OrA1SteppingWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, true},
         {REVISION_A1, true},
         {REVISION_B, false},
         {REVISION_C, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->isDefaultEngineTypeAdjustmentRequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->isAllocationSizeAdjustmentRequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->isPrefetchDisablingRequired(hwInfo));
    }
}

DG2TEST_F(Dg2HwInfoConfig, givenProgramPipeControlPriorToNonPipelinedStateCommandWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = false;

    EXPECT_TRUE(hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs));
}

DG2TEST_F(Dg2HwInfoConfig, givenProgramPipeControlPriorToNonPipelinedStateCommandWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenTrueIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(true);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    EXPECT_TRUE(hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs));
}

DG2TEST_F(Dg2HwInfoConfig, givenProgramPipeControlPriorToNonPipelinedStateCommandDisabledWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.ProgramPipeControlPriorToNonPipelinedStateCommand.set(0);

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    auto isRcs = true;

    EXPECT_FALSE(hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs));
}

DG2TEST_F(Dg2HwInfoConfig, givenHwInfoConfigWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnCcsThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = false;

    EXPECT_TRUE(hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs));
}

DG2TEST_F(Dg2HwInfoConfig, givenHwInfoConfigWithMultipleCSSWhenIsPipeControlPriorToNonPipelinedStateCommandsWARequiredIsCalledOnRcsThenFalseIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    auto hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    auto isRcs = true;

    EXPECT_FALSE(hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs));
}

DG2TEST_F(Dg2HwInfoConfig, givenDg2WhenIsBlitterForImagesSupportedIsCalledThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    EXPECT_TRUE(hwInfoConfig.isBlitterForImagesSupported());
}

DG2TEST_F(Dg2HwInfoConfig, givenB0rCSteppingWhenAskingIfTile64With3DSurfaceOnBCSIsSupportedThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 4> revisions = {
        {{REVISION_A0, false},
         {REVISION_A1, false},
         {REVISION_B, false},
         {REVISION_C, true}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->isTile64With3DSurfaceOnBCSSupported(hwInfo));
    }
}
