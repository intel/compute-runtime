/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(LkfHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_LAKEFIELD != productFamily) {
        return;
    }
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(8u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(64u, gtSystemInfo.EUCount);

    strConfig = "default";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(8u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(64u, gtSystemInfo.EUCount);

    strConfig = "erroneous";
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using LkfHwInfo = ::testing::Test;

LKFTEST_F(LkfHwInfo, givenHwInfoConfigStringThenAfterSetupResultingVmeIsDisabled) {
    HardwareInfo hwInfo;

    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(hwInfo.capabilityTable.supportsVme);
}

LKFTEST_F(LkfHwInfo, givenBoolWhenCallLkfHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    std::string strConfig = "1x8x8";

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, strConfig);

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
