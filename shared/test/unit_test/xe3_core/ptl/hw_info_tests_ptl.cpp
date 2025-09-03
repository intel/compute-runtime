/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/xe3_core/hw_cmds_ptl.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PtlHwInfoTest = ::testing::Test;

PTLTEST_F(PtlHwInfoTest, WhenGettingHardwareInfoThenPtlIsReturned) {
    EXPECT_EQ(IGFX_PTL, defaultHwInfo->platform.eProductFamily);
}

PTLTEST_F(PtlHwInfoTest, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValues) {
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
    PtlHwConfig::setupHardwareInfo(&hwInfo, false, releaseHelper.get());
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
    PtlHwConfig::setupHardwareInfo(&hwInfo, true, releaseHelper.get());
    EXPECT_TRUE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
    EXPECT_TRUE(featureTable.flags.ftrE2ECompression);
    EXPECT_EQ(featureTable.flags.ftrXe2Compression, releaseHelper->getFtrXe2Compression());
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
    EXPECT_FALSE(featureTable.flags.ftrTileY);
    EXPECT_EQ(1u, featureTable.ftrBcsInfo.to_ulong());
    EXPECT_TRUE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
}

PTLTEST_F(PtlHwInfoTest, whenCheckDirectSubmissionEnginesThenProperValuesAreSetToTrue) {
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
        case aub_stream::ENGINE_BCS:
            EXPECT_TRUE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_TRUE(directSubmissionEngines.data[i].useNonDefault);
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
