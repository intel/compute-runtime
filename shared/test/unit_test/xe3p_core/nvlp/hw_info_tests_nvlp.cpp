/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/test.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using NvlHwInfoTest = ::testing::Test;

NVLPTEST_F(NvlHwInfoTest, WhenGettingHardwareInfoThenNvlIsReturned) {
    EXPECT_EQ(IGFX_NVL, defaultHwInfo->platform.eProductFamily);
}

NVLPTEST_F(NvlHwInfoTest, givenBoolWhenCallNvlHardwareInfoSetupThenFeatureTableAndWorkaroundTableAreSetCorrect) {
    uint64_t configs[] = {0x200060008};

    bool boolValue[]{true, false};
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    for (auto &config : configs) {
        for (auto setParamBool : boolValue) {

            gtSystemInfo = {0};
            featureTable = {};
            workaroundTable = {};
            hardwareInfoSetup[productFamily](&hwInfo, setParamBool, config, releaseHelper.get());

            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrLinearCCS);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFlatPhysCCS);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSNode);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrCCSRing);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrPPGTT);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSVM);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrL3IACoherency);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrStandardMipTailFormat);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrUserModeTranslationTable);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTileMappedResource);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrFbc);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcHdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrAstcLdr2D);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuMidBatchPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrGpGpuThreadGroupLevelPreempt);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrTile64Optimization);
            EXPECT_EQ(false, featureTable.flags.ftrTileY);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrWalkerMTP);
            EXPECT_EQ(setParamBool, featureTable.flags.ftrSelectiveWmtp);
            EXPECT_TRUE(featureTable.flags.ftrHeaplessMode);

            EXPECT_EQ(setParamBool, workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
        }
    }
}

NVLPTEST_F(NvlHwInfoTest, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    auto releaseHelper = ReleaseHelper::create(hwInfo.ipVersion);
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrXe2Compression);
    EXPECT_FALSE(featureTable.flags.ftrXe2PlusTiling);
    EXPECT_FALSE(featureTable.flags.ftrPml5Support);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.flags.ftrHwSemaphore64);
    EXPECT_FALSE(featureTable.flags.ftrSelectiveWmtp);
    NvlHwConfig::setupHardwareInfo(&hwInfo, false, releaseHelper.get());
    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrXe2Compression);
    EXPECT_FALSE(featureTable.flags.ftrXe2PlusTiling);
    EXPECT_FALSE(featureTable.flags.ftrPml5Support);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.flags.ftrHwSemaphore64);
    EXPECT_FALSE(featureTable.flags.ftrSelectiveWmtp);
    NvlHwConfig::setupHardwareInfo(&hwInfo, true, releaseHelper.get());
    EXPECT_TRUE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
    EXPECT_TRUE(featureTable.flags.ftrE2ECompression);
    EXPECT_EQ(featureTable.flags.ftrXe2Compression, releaseHelper->getFtrXe2Compression());
    EXPECT_EQ(featureTable.flags.ftrHwSemaphore64, releaseHelper->isAvailableSemaphore64());
    EXPECT_TRUE(featureTable.flags.ftrXe2PlusTiling);
    EXPECT_TRUE(featureTable.flags.ftrPml5Support);
    EXPECT_TRUE(featureTable.flags.ftrCCSNode);
    EXPECT_TRUE(featureTable.flags.ftrCCSRing);
    EXPECT_TRUE(featureTable.flags.ftrPPGTT);
    EXPECT_TRUE(featureTable.flags.ftrSVM);
    EXPECT_TRUE(featureTable.flags.ftrL3IACoherency);
    EXPECT_TRUE(featureTable.flags.ftrIA32eGfxPTEs);
    EXPECT_TRUE(featureTable.flags.ftrStandardMipTailFormat);
    EXPECT_TRUE(featureTable.flags.ftrTranslationTable);
    EXPECT_TRUE(featureTable.flags.ftrUserModeTranslationTable);
    EXPECT_TRUE(featureTable.flags.ftrTileMappedResource);
    EXPECT_TRUE(featureTable.flags.ftrFbc);
    EXPECT_TRUE(featureTable.flags.ftrAstcHdr2D);
    EXPECT_TRUE(featureTable.flags.ftrAstcLdr2D);
    EXPECT_TRUE(featureTable.flags.ftrSelectiveWmtp);
    EXPECT_FALSE(featureTable.flags.ftrTileY);
    EXPECT_EQ(1u, featureTable.ftrBcsInfo.to_ulong());
    EXPECT_TRUE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
}
