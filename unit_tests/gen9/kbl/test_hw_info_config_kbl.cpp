/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(KblHwInfoConfig, givenHwInfoErrorneousConfigString) {
    if (IGFX_KABYLAKE != productFamily) {
        return;
    }
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    std::string strConfig = "erroneous";
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using KblHwInfo = ::testing::Test;

KBLTEST_F(KblHwInfo, givenBoolWhenCallKblHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x3x8",
        "2x3x8",
        "3x3x8",
        "1x2x6",
        "1x3x6"};
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

            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.ftrVEBOX);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);
            EXPECT_EQ(setParamBool, featureTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, featureTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, workaroundTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, workaroundTable.waLosslessCompressionSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
            EXPECT_EQ(false, workaroundTable.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(false, workaroundTable.waEncryptedEdramOnlyPartials);
            EXPECT_EQ(false, workaroundTable.waForcePcBbFullCfgRestore);

            platform.usRevId = 1;
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, workaroundTable.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(true, workaroundTable.waEncryptedEdramOnlyPartials);
            EXPECT_EQ(true, workaroundTable.waForcePcBbFullCfgRestore);
        }
    }
}
