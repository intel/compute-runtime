/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

TEST(BdwHwInfoConfig, givenInvalidSystemInfoWhenSettingHardwareInfoThenExpectThrow) {
    if (IGFX_BROADWELL != productFamily) {
        return;
    }
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    uint64_t config = 0xdeadbeef;
    gtSystemInfo = {0};
    EXPECT_ANY_THROW(hardwareInfoSetup[productFamily](&hwInfo, false, config));
    EXPECT_EQ(0u, gtSystemInfo.SliceCount);
    EXPECT_EQ(0u, gtSystemInfo.SubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.DualSubSliceCount);
    EXPECT_EQ(0u, gtSystemInfo.EUCount);
}

using BdwHwInfo = ::testing::Test;

BDWTEST_F(BdwHwInfo, givenBoolWhenCallBdwHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {
        0x100030008,
        0x200030008,
        0x100020006,
        0x100030006};
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
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc2AddressTranslation);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcBlitterTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbcCpuTracking);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileY);

            EXPECT_EQ(setParamBool, workaroundTable.flags.waDisableLSQCROPERFforOCL);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waReportPerfCountUseGlobalContextID);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waUseVAlign16OnTileXYBpp816);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption);
            EXPECT_EQ(setParamBool, workaroundTable.flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads);
        }
    }
}

BDWTEST_F(BdwHwInfo, givenHwInfoConfigStringThenAfterSetupResultingVmeIsDisabled) {
    HardwareInfo hwInfo = *defaultHwInfo;

    uint64_t config = 0x0;
    hardwareInfoSetup[productFamily](&hwInfo, false, config);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcTextureSampler);
    EXPECT_FALSE(hwInfo.capabilityTable.ftrSupportsVmeAvcPreemption);
    EXPECT_FALSE(hwInfo.capabilityTable.supportsVme);
}

using CompilerHwInfoConfigHelperTestsBdw = ::testing::Test;
BDWTEST_F(CompilerHwInfoConfigHelperTestsBdw, givenBdwWhenIsStatelessToStatefulBufferOffsetSupportedIsCalledThenReturnsTrue) {
    EXPECT_FALSE(CompilerHwInfoConfig::get(productFamily)->isStatelessToStatefulBufferOffsetSupported());
}
