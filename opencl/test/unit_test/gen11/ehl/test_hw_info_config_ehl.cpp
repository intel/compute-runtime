/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/default_hw_info.h"

#include "test.h"
using namespace NEO;

using EhlHwInfo = ::testing::Test;

EHLTEST_F(EhlHwInfo, givenHwInfoConfigStringThenAfterSetupResultingVmeIsDisabled) {
    HardwareInfo hwInfo = *defaultHwInfo;

    hardwareInfoSetup[productFamily](&hwInfo, false, 0x100040008);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(hwInfo.capabilityTable.supportsVme);
}

EHLTEST_F(EhlHwInfo, givenBoolWhenCallEhlHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, 0x100040008);

        EXPECT_EQ(setParamBool, featureTable.ftrL3IACoherency);
        EXPECT_EQ(setParamBool, featureTable.ftrPPGTT);
        EXPECT_EQ(setParamBool, featureTable.ftrSVM);
        EXPECT_EQ(setParamBool, featureTable.ftrIA32eGfxPTEs);
        EXPECT_EQ(setParamBool, featureTable.ftrStandardMipTailFormat);
        EXPECT_EQ(setParamBool, featureTable.ftrDisplayYTiling);
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
        EXPECT_EQ(setParamBool, featureTable.ftrGpGpuMidThreadLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.ftrPerCtxtPreemptionGranularityControl);

        EXPECT_EQ(setParamBool, workaroundTable.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.waReportPerfCountUseGlobalContextID);
    }
}

HWTEST_EXCLUDE_PRODUCT(DeviceFactoryTest, givenInvalidHwConfigStringWhenPrepareDeviceEnvironmentsForProductFamilyOverrideThenThrowsException, IGFX_ELKHARTLAKE);
