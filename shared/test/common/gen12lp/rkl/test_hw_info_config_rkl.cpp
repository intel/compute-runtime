/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;

using RklHwInfo = ::testing::Test;

RKLTEST_F(RklHwInfo, givenBoolWhenCallRklHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100020010};
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

            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrEnableGuC);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftr3dMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPerCtxtPreemptionGranularityControl);

            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waEnablePreemptionGranularityControlByUMD);
        }
    }
}

RKLTEST_F(RklHwInfo, whenPlatformIsRklThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

RKLTEST_F(RklHwInfo, givenRklWhenCheckFtrSupportsInteger64BitAtomicsThenReturnFalse) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSupportsInteger64BitAtomics);
}

RKLTEST_F(RklHwInfo, givenRklWhenCheckL0ThenReturnTrue) {
    const HardwareInfo &hardwareInfo = RKL::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.levelZeroSupported);
}

RKLTEST_F(RklHwInfo, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getProductConfigFromHwInfo(hwInfo), AOT::RKL);
}