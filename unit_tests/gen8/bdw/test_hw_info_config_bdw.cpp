/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(BdwHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_BROADWELL != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gtSystemInfo = {0};
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gtSystemInfo;

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

    strConfig = "1x2x6";
    gtSystemInfo = {0};
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
    EXPECT_EQ(23u, gtSystemInfo.EUCount);

    strConfig = "erroneous";
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));

    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using BdwHwInfo = ::testing::Test;

BDWTEST_F(BdwHwInfo, givenBoolWhenCallBdwHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x3x8",
        "2x3x8",
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
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, pSkuTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, pSkuTable.ftrSVM);
            EXPECT_EQ(setParamBool, pSkuTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, pSkuTable.ftrTileY);

            EXPECT_EQ(setParamBool, pWaTable.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(setParamBool, pWaTable.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, pWaTable.waUseVAlign16OnTileXYBpp816);
            EXPECT_EQ(setParamBool, pWaTable.waModifyVFEStateAfterGPGPUPreemption);
            EXPECT_EQ(setParamBool, pWaTable.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
        }
    }
}
