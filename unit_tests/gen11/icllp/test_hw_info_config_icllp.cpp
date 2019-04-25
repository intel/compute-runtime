/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(IcllpHwInfoConfig, givenHwInfoConfigStringThenAfterSetupResultingHwInfoIsCorrect) {
    if (IGFX_ICELAKE_LP != productFamily) {
        return;
    }
    GT_SYSTEM_INFO gInfo = {0};
    HardwareInfo hwInfo;
    hwInfo.pSysInfo = &gInfo;

    std::string strConfig = "1x8x8";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(8u, gInfo.SubSliceCount);
    EXPECT_EQ(63u, gInfo.EUCount);

    strConfig = "1x4x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(4u, gInfo.SubSliceCount);
    EXPECT_EQ(31u, gInfo.EUCount);

    strConfig = "1x6x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(6u, gInfo.SubSliceCount);
    EXPECT_EQ(47u, gInfo.EUCount);

    strConfig = "1x1x8";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(1u, gInfo.SubSliceCount);
    EXPECT_EQ(8u, gInfo.EUCount);

    strConfig = "default";
    gInfo = {0};
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_EQ(1u, gInfo.SliceCount);
    EXPECT_EQ(8u, gInfo.SubSliceCount);
    EXPECT_EQ(63u, gInfo.EUCount);

    strConfig = "erroneous";
    gInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, strConfig));
    EXPECT_EQ(0u, gInfo.SliceCount);
    EXPECT_EQ(0u, gInfo.SubSliceCount);
    EXPECT_EQ(0u, gInfo.EUCount);
}

using IcllpHwInfo = ::testing::Test;

ICLLPTEST_F(IcllpHwInfo, givenBoolWhenCallIcllpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    std::string strConfig[] = {
        "1x8x8",
        "1x4x8",
        "1x6x8",
        "1x1x8"};
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

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gInfo = {0};
            pSkuTable = {};
            pWaTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

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
}
