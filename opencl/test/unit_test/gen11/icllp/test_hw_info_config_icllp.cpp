/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using IcllpHwInfoConfig = ::testing::Test;

ICLLPTEST_F(IcllpHwInfoConfig, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {
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

ICLLPTEST_F(IcllpHwInfoConfig, givenHwInfoConfigWhenAskedIfAdditionalMediaSamplerProgrammingIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isAdditionalMediaSamplerProgrammingRequired());
}

ICLLPTEST_F(IcllpHwInfoConfig, givenHwInfoConfigWhenAskedIfInitialFlagsProgrammingIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isInitialFlagsProgrammingRequired());
}

ICLLPTEST_F(IcllpHwInfoConfig, givenHwInfoConfigWhenAskedIfReturnedCmdSizeForMediaSamplerAdjustmentIsRequiredThenTrueIsReturned) {
    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isReturnedCmdSizeForMediaSamplerAdjustmentRequired());
}

using IcllpHwInfo = ::testing::Test;

ICLLPTEST_F(IcllpHwInfo, givenBoolWhenCallIcllpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100080008,
        0x100040008,
        0x100060008};
    bool boolValue[]{
        true, false};

    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto config : configs) {
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
            EXPECT_EQ(setParamBool, featureTable.flags.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPerCtxtPreemptionGranularityControl);

            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waReportPerfCountUseGlobalContextID);
        }
    }
}
