/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "platforms.h"

using namespace NEO;

using AdlpHwInfo = ::testing::Test;

ADLPTEST_F(AdlpHwInfo, givenBoolWhenCallAdlpHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    static bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    uint64_t config = 0x0;
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
        EXPECT_FALSE(featureTable.flags.ftrTileY);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftr3dMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPerCtxtPreemptionGranularityControl);

        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(setParamBool, workaroundTable.flags.waEnablePreemptionGranularityControlByUMD);
        EXPECT_EQ(setParamBool, workaroundTable.flags.waUntypedBufferCompression);
    }
}

ADLPTEST_F(AdlpHwInfo, whenPlatformIsAdlpThenExpectSvmIsSet) {
    const HardwareInfo &hardwareInfo = ADLP::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.ftrSvm);
}

ADLPTEST_F(AdlpHwInfo, givenAdlpWhenCheckL0ThenReturnTrue) {
    const HardwareInfo &hardwareInfo = ADLP::hwInfo;
    EXPECT_TRUE(hardwareInfo.capabilityTable.levelZeroSupported);
}

ADLPTEST_F(AdlpHwInfo, givenHwInfoConfigWhenGetProductConfigThenCorrectMatchIsFound) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_EQ(hwInfoConfig.getProductConfigFromHwInfo(hwInfo), AOT::ADL_P);
}

ADLPTEST_F(AdlpHwInfo, givenHwInfoConfigWhenGettingEvictWhenNecessaryFlagSupportedThenExpectTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    EXPECT_TRUE(hwInfoConfig.isEvictionWhenNecessaryFlagSupported());
}
