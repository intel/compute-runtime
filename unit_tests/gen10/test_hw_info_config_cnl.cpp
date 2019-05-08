/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(CnlHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_CANNONLAKE != productFamily) {
        return;
    }
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    gtSystemInfo = {0};

    std::string strConfig = "1x2x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(2u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(15u, gtSystemInfo.EUCount);

    strConfig = "1x3x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(23u, gtSystemInfo.EUCount);

    strConfig = "2x4x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(2u, gtSystemInfo.SliceCount);
    EXPECT_EQ(8u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(31u, gtSystemInfo.EUCount);

    strConfig = "2x5x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(2u, gtSystemInfo.SliceCount);
    EXPECT_EQ(10u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(39u, gtSystemInfo.EUCount);

    strConfig = "4x9x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(4u, gtSystemInfo.SliceCount);
    EXPECT_EQ(36u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(71u, gtSystemInfo.EUCount);

    strConfig = "default";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(2u, gtSystemInfo.SliceCount);
    EXPECT_EQ(10u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(39u, gtSystemInfo.EUCount);

    strConfig = "erroneous";
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using CnlHwInfo = ::testing::Test;

CNLTEST_F(CnlHwInfo, givenBoolWhenCallCnlHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x2x8",
        "1x3x8",
        "2x5x8",
        "2x4x8",
        "4x9x8"};
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;
    PLATFORM &platform = hwInfo.platform;

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};

            platform.usRevId = 9;
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);
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
            EXPECT_EQ(setParamBool, featureTable.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
            EXPECT_EQ(false, workaroundTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(false, workaroundTable.waEncryptedEdramOnlyPartials);

            platform.usRevId = 0;
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, workaroundTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(true, workaroundTable.waEncryptedEdramOnlyPartials);
        }
    }
}
