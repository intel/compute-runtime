/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"

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

            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.ftrStandardMipTailFormat);
            EXPECT_EQ(setParamBool, featureTable.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, featureTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);

            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
        }
    }
}
