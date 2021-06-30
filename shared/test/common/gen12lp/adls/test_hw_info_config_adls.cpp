/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "test.h"
using namespace NEO;

using AdlsHwInfo = ::testing::Test;

ADLSTEST_F(AdlsHwInfo, givenBoolWhenCallAdlsHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t config = 0x100020016;
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
        EXPECT_EQ(setParamBool, featureTable.ftr3dMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);

        EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.waEnablePreemptionGranularityControlByUMD);
        EXPECT_EQ(setParamBool, workaroundTable.waUntypedBufferCompression);
    }
}

ADLSTEST_F(AdlsHwInfo, whenPlatformIsAdlsThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = ADLS::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

ADLSTEST_F(AdlsHwInfo, givenAdlsWhenCheckL0ThenReturnTrue) {
    const HardwareInfo &hardwareInfo = ADLS::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.levelZeroSupported);
}
