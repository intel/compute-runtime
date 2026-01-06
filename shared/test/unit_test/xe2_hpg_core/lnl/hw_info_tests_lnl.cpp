/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe2_hpg_core/hw_cmds_lnl.h"
#include "shared/source/xe2_hpg_core/hw_info_lnl.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using LnlHwInfoTest = ::testing::Test;

LNLTEST_F(LnlHwInfoTest, WhenGettingHardwareInfoThenLnlIsReturned) {
    EXPECT_EQ(IGFX_LUNARLAKE, defaultHwInfo->platform.eProductFamily);
}

LNLTEST_F(LnlHwInfoTest, givenBoolWhenCallLnlHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    bool boolValue[]{
        true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto setParamBool : boolValue) {

        gtSystemInfo = {0};
        featureTable = {};
        workaroundTable = {};
        hardwareInfoSetup[productFamily](&hwInfo, setParamBool, compilerProductHelper->getHwInfoConfig(hwInfo), releaseHelper.get());

        EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrLinearCCS);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFlatPhysCCS);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSNode);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSRing);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrE2ECompression);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrIA32eGfxPTEs);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrTile64Optimization);
        EXPECT_EQ(false, featureTable.flags.ftrXe2Compression);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrXe2PlusTiling);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrPml5Support);
        EXPECT_EQ(false, featureTable.flags.ftrTileY);
        EXPECT_EQ(setParamBool, featureTable.flags.ftrWalkerMTP);
        EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        EXPECT_EQ(1u, featureTable.ftrBcsInfo.to_ulong());
        EXPECT_FALSE(featureTable.flags.ftrHeaplessMode);
    }
}

LNLTEST_F(LnlHwInfoTest, whenSetupHardwareInfoBaseThenCorrectValuesOfCCSAndMultiTileInfoAreSet) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    hardwareInfoSetup[productFamily](&hwInfo, false, compilerProductHelper->getHwInfoConfig(hwInfo), releaseHelper.get());
    EXPECT_FALSE(gtSystemInfo.MultiTileArchInfo.IsValid);
    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_TRUE(1u == gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_TRUE(0b1u == gtSystemInfo.CCSInfo.Instances.CCSEnableMask);
}

LNLTEST_F(LnlHwInfoTest, whenCheckDirectSubmissionEnginesThenProperValuesAreSetToTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &directSubmissionEngines = hwInfo.capabilityTable.directSubmissionEngines;

    for (uint32_t i = 0; i < aub_stream::NUM_ENGINES; i++) {
        switch (i) {
        case aub_stream::ENGINE_CCS:
            EXPECT_TRUE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_TRUE(directSubmissionEngines.data[i].useRootDevice);
            break;
        default:
            EXPECT_FALSE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_FALSE(directSubmissionEngines.data[i].useRootDevice);
        }
    }
}
