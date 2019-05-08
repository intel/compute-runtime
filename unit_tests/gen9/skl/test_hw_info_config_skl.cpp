/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(SklHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_SKYLAKE != productFamily) {
        return;
    }
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    gtSystemInfo = {0};

    std::string strConfig = "1x3x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(23u, gtSystemInfo.EUCount);

    strConfig = "2x3x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(2u, gtSystemInfo.SliceCount);
    EXPECT_EQ(6u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(47u, gtSystemInfo.EUCount);

    strConfig = "3x3x8";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(3u, gtSystemInfo.SliceCount);
    EXPECT_EQ(9u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(71u, gtSystemInfo.EUCount);

    strConfig = "1x2x6";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(2u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(11u, gtSystemInfo.EUCount);

    strConfig = "1x3x6";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(17u, gtSystemInfo.EUCount);

    strConfig = "default";
    gtSystemInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gtSystemInfo.SliceCount);
    EXPECT_EQ(3u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(23u, gtSystemInfo.EUCount);

    strConfig = "erroneous";

    gtSystemInfo = {0};

    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using SklHwInfo = ::testing::Test;

SKLTEST_F(SklHwInfo, givenBoolWhenCallSklHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
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
    PLATFORM &pPlatform = hwInfo.platform;

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            pPlatform.usRevId = 9;
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
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
            EXPECT_EQ(setParamBool, featureTable.ftrVEBOX);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);
            EXPECT_EQ(false, featureTable.ftrSingleVeboxSlice);
            EXPECT_EQ(false, featureTable.ftrVcs2);

            EXPECT_EQ(setParamBool, workaroundTable.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, workaroundTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(setParamBool, workaroundTable.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, workaroundTable.waLosslessCompressionSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waEncryptedEdramOnlyPartials);
            EXPECT_EQ(setParamBool, workaroundTable.waDisableEdramForDisplayRT);
            EXPECT_EQ(setParamBool, workaroundTable.waForcePcBbFullCfgRestore);
            EXPECT_EQ(setParamBool, workaroundTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
            EXPECT_EQ(false, workaroundTable.waCompressedResourceRequiresConstVA21);
            EXPECT_EQ(false, workaroundTable.waDisablePerCtxtPreemptionGranularityControl);
            EXPECT_EQ(false, workaroundTable.waModifyVFEStateAfterGPGPUPreemption);
            EXPECT_EQ(false, workaroundTable.waCSRUncachable);

            pPlatform.usRevId = 1;
            workaroundTable = {};
            featureTable = {};
            featureTable.ftrGT1 = true;
            featureTable.ftrGT3 = true;
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, workaroundTable.waCompressedResourceRequiresConstVA21);
            EXPECT_EQ(true, workaroundTable.waDisablePerCtxtPreemptionGranularityControl);
            EXPECT_EQ(true, workaroundTable.waModifyVFEStateAfterGPGPUPreemption);
            EXPECT_EQ(true, workaroundTable.waCSRUncachable);
            EXPECT_EQ(true, featureTable.ftrSingleVeboxSlice);
            EXPECT_EQ(true, featureTable.ftrVcs2);

            workaroundTable = {};
            featureTable = {};
            featureTable.ftrGT2 = true;
            featureTable.ftrGT4 = true;
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, featureTable.ftrSingleVeboxSlice);
            EXPECT_EQ(true, featureTable.ftrVcs2);

            workaroundTable = {};
            featureTable = {};
            featureTable.ftrGT1 = true;
            featureTable.ftrGT2 = true;
            featureTable.ftrGT3 = true;
            featureTable.ftrGT4 = true;
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, featureTable.ftrSingleVeboxSlice);
            EXPECT_EQ(true, featureTable.ftrVcs2);
        }
    }
}
