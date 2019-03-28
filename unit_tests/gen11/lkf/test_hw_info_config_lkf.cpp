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
    GT_SYSTEM_INFO gInfo = {0};
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gInfo;

    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(8u, gInfo.SubSliceCount);
    EXPECT_EQ(64u, gInfo.EUCount);

    strConfig = "default";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(8u, gInfo.SubSliceCount);
    EXPECT_EQ(64u, gInfo.EUCount);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gInfo.SliceCount);
    EXPECT_EQ(0u, gInfo.SubSliceCount);
    EXPECT_EQ(0u, gInfo.EUCount);
}

using LkfHwInfo = ::testing::Test;

LKFTEST_F(LkfHwInfo, givenBoolWhenCallLkfHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    GT_SYSTEM_INFO gInfo = {0};
    FeatureTable pSkuTable;
    WorkaroundTable pWaTable;
    PLATFORM pPlatform;
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gInfo;
    hwInfo.pSkuTable = &pSkuTable;
    hwInfo.pWaTable = &pWaTable;
    hwInfo.pPlatform = &pPlatform;

    std::string strConfig = "1x8x8";

    for (auto setParamBool : boolValue) {

        gInfo = {0};
        pSkuTable = {};
        pWaTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, strConfig);

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

        EXPECT_EQ(setParamBool, pWaTable.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, pWaTable.waReportPerfCountUseGlobalContextID);
    }
}
