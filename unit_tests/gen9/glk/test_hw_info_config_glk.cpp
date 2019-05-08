/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(GlkHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_GEMINILAKE != productFamily) {
        return;
    }
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    gtSystemInfo = {0};

    std::string strConfig = "1x2x6";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(2u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(12u, gtSystemInfo.EUCount);

    strConfig = "1x3x6";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(18u, gtSystemInfo.EUCount);

    strConfig = "default";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(18u, gtSystemInfo.EUCount);

    strConfig = "erroneous";
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using GlkHwInfo = ::testing::Test;

GLKTEST_F(GlkHwInfo, givenBoolWhenCallGlkHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x2x6",
        "1x3x6"};
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
            EXPECT_EQ(false, featureTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);
            EXPECT_EQ(setParamBool, featureTable.ftrLCIA);
            EXPECT_EQ(setParamBool, featureTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.ftrULT);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.waLLCCachingUnsupported);
            EXPECT_EQ(setParamBool, workaroundTable.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, workaroundTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, workaroundTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.waForcePcBbFullCfgRestore);
            EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
        }
    }
}
