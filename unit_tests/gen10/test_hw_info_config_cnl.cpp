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
    GT_SYSTEM_INFO gtSystemInfo = {0};
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gtSystemInfo;

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

            EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuMidThreadLevelPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftr3dObjectLevelPreempt);
            EXPECT_EQ(setParamBool, pSkuTable.ftrPerCtxtPreemptionGranularityControl);
            EXPECT_EQ(setParamBool, pSkuTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, pSkuTable.ftrSVM);
            EXPECT_EQ(setParamBool, pSkuTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, pSkuTable.ftrStandardMipTailFormat);
            EXPECT_EQ(setParamBool, pSkuTable.ftrDisplayYTiling);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, pSkuTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, pSkuTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, pSkuTable.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTileY);

            EXPECT_EQ(setParamBool, pWaTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, pWaTable.waSendMIFLUSHBeforeVFE);
            EXPECT_EQ(setParamBool, pWaTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, pWaTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
            EXPECT_EQ(false, pWaTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(false, pWaTable.waEncryptedEdramOnlyPartials);

            pPlatform.usRevId = 0;
            pWaTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, true, config);

            EXPECT_EQ(true, pWaTable.waFbcLinearSurfaceStride);
            EXPECT_EQ(true, pWaTable.waEncryptedEdramOnlyPartials);
        }
    }
}
