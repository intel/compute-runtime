/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

TEST(GlkHwInfoConfig, GivenIncorrectDataWhenConfiguringHwInfoThenErrorIsReturned) {
    if (IGFX_GEMINILAKE != productFamily) {
        return;
    }
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    gtSystemInfo = {0};

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using GlkHwInfo = ::testing::Test;

GLKTEST_F(GlkHwInfo, givenBoolWhenCallGlkHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100020006,
        0x100030006};
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPerCtxtPreemptionGranularityControl);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrLCIA);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrULT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.flags.waLLCCachingUnsupported);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waForcePcBbFullCfgRestore);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
        }
    }
}
