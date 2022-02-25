/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Dg1HwInfoConfig = ::testing::Test;

DG1TEST_F(Dg1HwInfoConfig, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {
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

DG1TEST_F(Dg1HwInfoConfig, givenA0SteppingAndDg1PlatformWhenAskingIfWAIsRequiredThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(productFamily);
    std::array<std::pair<uint32_t, bool>, 2> revisions = {
        {{REVISION_A0, true},
         {REVISION_B, false}}};

    for (const auto &[revision, paramBool] : revisions) {
        auto hwInfo = *defaultHwInfo;
        hwInfo.platform.usRevId = hwInfoConfig->getHwRevIdFromStepping(revision, hwInfo);

        hwInfoConfig->configureHardwareCustom(&hwInfo, nullptr);

        EXPECT_EQ(paramBool, hwInfoConfig->pipeControlWARequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->imagePitchAlignmentWARequired(hwInfo));
        EXPECT_EQ(paramBool, hwInfoConfig->isForceEmuInt32DivRemSPWARequired(hwInfo));
    }
}

DG1TEST_F(Dg1HwInfoConfig, givenHwInfoConfigWhenAskedIfStorageInfoAdjustmentIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isStorageInfoAdjustmentRequired());
}

DG1TEST_F(Dg1HwInfoConfig, givenHwInfoConfigWhenAskedIf3DPipelineSelectWAIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.is3DPipelineSelectWARequired());
}

using Dg1HwInfo = ::testing::Test;

DG1TEST_F(Dg1HwInfo, givenBoolWhenCallDg1HardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t config = 0x100060010;
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
        EXPECT_EQ(setParamBool, featureTable.flags.ftrLocalMemory);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrEnableGuC);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc2AddressTranslation);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcBlitterTracking);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcCpuTracking);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftr3dMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPerCtxtPreemptionGranularityControl);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.flags.waEnablePreemptionGranularityControlByUMD);
    }
}

DG1TEST_F(Dg1HwInfo, givenDg1WhenObtainingBlitterPreferenceThenReturnTrue) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const auto &hardwareInfo = DG1::hwInfo;

    EXPECT_TRUE(hwInfoConfig.obtainBlitterPreference(hardwareInfo));
}

DG1TEST_F(Dg1HwInfo, whenPlatformIsDg1ThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = DG1::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

DG1TEST_F(Dg1HwInfo, whenConfigureHwInfoThenBlitterSupportIsEnabled) {
    auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    auto hardwareInfo = *defaultHwInfo;

    hardwareInfo.capabilityTable.blitterOperationsSupported = false;
    hwInfoConfig.configureHardwareCustom(&hardwareInfo, nullptr);

    EXPECT_TRUE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

DG1TEST_F(Dg1HwInfo, givenDg1WhenObtainingFullBlitterSupportThenReturnFalse) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    const auto &hardwareInfo = DG1::hwInfo;

    EXPECT_FALSE(hwInfoConfig.isBlitterFullySupported(hardwareInfo));
}

DG1TEST_F(Dg1HwInfo, whenOverrideGfxPartitionLayoutForWslThenReturnTrue) {
    auto hwInfoConfig = HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig->overrideGfxPartitionLayoutForWsl());
}
