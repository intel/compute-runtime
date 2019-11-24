/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

using namespace NEO;

TEST(TgllpHwInfoConfig, givenHwInfoErrorneousConfigString) {
    if (IGFX_TIGERLAKE_LP != productFamily) {
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

using TgllpHwInfo = ::testing::Test;

TGLLPTEST_F(TgllpHwInfo, givenBoolWhenCallTgllpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    std::string strConfig[] = {
        "1x6x16",
        "1x2x16"};

    for (auto &config : strConfig) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config);

            EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.ftrStandardMipTailFormat);
            EXPECT_EQ(setParamBool, featureTable.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.ftrEnableGuC);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.ftrTileY);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.ftrAstcLdr2D);

            EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.waEnablePreemptionGranularityControlByUMD);
            EXPECT_EQ(setParamBool, workaroundTable.waUntypedBufferCompression);
        }
    }
}

TGLLPTEST_F(TgllpHwInfo, givenHwInfoConfigStringThenAfterSetupResultingVmeIsDisabled) {
    HardwareInfo hwInfo;

    std::string strConfig = "1x6x16";
    hardwareInfoSetup[productFamily](&hwInfo, false, strConfig);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(hwInfo.capabilityTable.supportsVme);
}

TGLLPTEST_F(TgllpHwInfo, givenA0SteppingWhenWaTableIsInitializedThenWaUseOffsetToSkipSetFFIDGPIsSet) {
    HardwareInfo hwInfo;
    hwInfo.platform.usRevId = REVISION_A0;
    TGLLP::setupFeatureAndWorkaroundTable(&hwInfo);

    EXPECT_TRUE(hwInfo.workaroundTable.waUseOffsetToSkipSetFFIDGP);
}

TGLLPTEST_F(TgllpHwInfo, givenBSteppingWhenWaTableIsInitializedThenWaUseOffsetToSkipSetFFIDGPIsNotSet) {
    HardwareInfo hwInfo;
    hwInfo.platform.usRevId = REVISION_B;
    TGLLP::setupFeatureAndWorkaroundTable(&hwInfo);

    EXPECT_FALSE(hwInfo.workaroundTable.waUseOffsetToSkipSetFFIDGP);
}

TGLLPTEST_F(TgllpHwInfo, givenA0SteppingWhenWaTableIsInitializedThenWaForceDefaultRCSEngineIsSet) {
    HardwareInfo hwInfo;
    hwInfo.platform.usRevId = REVISION_A0;
    TGLLP::setupFeatureAndWorkaroundTable(&hwInfo);

    EXPECT_TRUE(hwInfo.workaroundTable.waForceDefaultRCSEngine);
}

TGLLPTEST_F(TgllpHwInfo, givenBSteppingWhenWaTableIsInitializedThenWaForceDefaultRCSEngineIsNotSet) {
    HardwareInfo hwInfo;
    hwInfo.platform.usRevId = REVISION_B;
    TGLLP::setupFeatureAndWorkaroundTable(&hwInfo);

    EXPECT_FALSE(hwInfo.workaroundTable.waForceDefaultRCSEngine);
}
