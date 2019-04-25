/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(BxtHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_BROXTON != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gtSystemInfo = {0};
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gtSystemInfo;

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

using BxtHwInfo = ::testing::Test;

BXTTEST_F(BxtHwInfo, givenBoolWhenCallBxtHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x2x6",
        "1x3x6"};
    bool boolValue[]{
        true, false};
    GT_SYSTEM_INFO gtSystemInfo = {0};
    FeatureTable pSkuTable;
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gtSystemInfo;
    hwInfo.pSkuTable = &pSkuTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            pSkuTable = {};
            pWaTable = {};
            pPlatform.usRevId = 9;
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, pSkuTable.ftrVEBOX);
            EXPECT_EQ(setParamBool, pSkuTable.ftrULT);
            EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftrPerCtxtPreemptionGranularityControl);
            EXPECT_EQ(setParamBool, pSkuTable.ftrLCIA);
            EXPECT_EQ(setParamBool, pSkuTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, pSkuTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, pSkuTable.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, pSkuTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, pSkuTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTileY);
            EXPECT_EQ(setParamBool, pSkuTable.ftrGttCacheInvalidation);

            EXPECT_EQ(setParamBool, pWaTable.waLLCCachingUnsupported);
            EXPECT_EQ(setParamBool, pWaTable.waMsaa8xTileYDepthPitchAlignment);
            EXPECT_EQ(setParamBool, pWaTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(setParamBool, pWaTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, pWaTable.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, pWaTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, pWaTable.waForcePcBbFullCfgRestore);
            EXPECT_EQ(setParamBool, pWaTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, pWaTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);

            pPlatform.usRevId = 1;
            pSkuTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(false, pSkuTable.ftrGttCacheInvalidation);
        }
    }
}
