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
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.pSysInfo;

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

LKFTEST_F(LkfHwInfo, givenBoolWhenCallLkfHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.pSysInfo;
    FeatureTable &pSkuTable = hwInfo.pSkuTable;
    WorkaroundTable &pWaTable = hwInfo.pWaTable;

    std::string strConfig = "1x8x8";

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        pSkuTable = {};
        pWaTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, strConfig);

        EXPECT_EQ(setParamBool, pSkuTable.ftrL3IACoherency);
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
        EXPECT_EQ(setParamBool, pSkuTable.ftrTileY);
        EXPECT_EQ(setParamBool, pSkuTable.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, pSkuTable.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, pSkuTable.ftr3dMidBatchPreempt);
        EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuMidThreadLevelPreempt);
        EXPECT_EQ(setParamBool, pSkuTable.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, pSkuTable.ftrPerCtxtPreemptionGranularityControl);

        EXPECT_EQ(setParamBool, pWaTable.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, pWaTable.waReportPerfCountUseGlobalContextID);
    }
}
