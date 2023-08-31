/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using MtlHwInfoTests = ::testing::Test;

MTLTEST_F(MtlHwInfoTests, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;

    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
    EXPECT_FALSE(workaroundTable.flags.waUntypedBufferCompression);

    MtlHwConfig::setupHardwareInfo(&hwInfo, false, *compilerProductHelper);
    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
    EXPECT_FALSE(workaroundTable.flags.waUntypedBufferCompression);

    MtlHwConfig::setupHardwareInfo(&hwInfo, true, *compilerProductHelper);
    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_TRUE(featureTable.flags.ftrCCSNode);
    EXPECT_TRUE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_TRUE(workaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface);
    EXPECT_TRUE(workaroundTable.flags.waUntypedBufferCompression);
}

MTLTEST_F(MtlHwInfoTests, givenMtlCapabilityTableWhenCheckDirectSubmissionEnginesThenProperValuesAreSetToTrue) {
    HardwareInfo hwInfo = *defaultHwInfo;
    const auto &directSubmissionEngines = hwInfo.capabilityTable.directSubmissionEngines;

    for (uint32_t i = 0; i < aub_stream::NUM_ENGINES; i++) {
        switch (i) {
        case aub_stream::ENGINE_CCS:
            EXPECT_TRUE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_TRUE(directSubmissionEngines.data[i].useRootDevice);
            EXPECT_FALSE(directSubmissionEngines.data[i].useInternal);
            EXPECT_FALSE(directSubmissionEngines.data[i].useLowPriority);
            break;
        default:
            EXPECT_FALSE(directSubmissionEngines.data[i].engineSupported);
            EXPECT_FALSE(directSubmissionEngines.data[i].submitOnInit);
            EXPECT_FALSE(directSubmissionEngines.data[i].useNonDefault);
            EXPECT_FALSE(directSubmissionEngines.data[i].useRootDevice);
            EXPECT_FALSE(directSubmissionEngines.data[i].useInternal);
            EXPECT_FALSE(directSubmissionEngines.data[i].useLowPriority);
        }
    }
}

MTLTEST_F(MtlHwInfoTests, WhenSetupHardwareInfoThenCorrectValuesOfCCSAndMultiTileInfoAreSet) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    MtlHwConfig::setupHardwareInfo(&hwInfo, false, *compilerProductHelper);

    EXPECT_FALSE(gtSystemInfo.MultiTileArchInfo.IsValid);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_TRUE(1u == gtSystemInfo.CCSInfo.NumberOfCCSEnabled);
    EXPECT_TRUE(0b1u == gtSystemInfo.CCSInfo.Instances.CCSEnableMask);
}

MTLTEST_F(MtlHwInfoTests, GivenEmptyHwInfoForUnitTestsWhenSetupHardwareInfoIsCalledThenNonZeroValuesAreSet) {
    HardwareInfo hwInfoToSet = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfoToSet.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfoToSet.gtSystemInfo;
    gtSystemInfo = {};

    MtlHwConfig::setupHardwareInfo(&hwInfoToSet, false, *compilerProductHelper);

    EXPECT_GT_VAL(gtSystemInfo.SliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.DualSubSliceCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.EUCount, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxEuPerSubSlice, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxSlicesSupported, 0u);
    EXPECT_GT_VAL(gtSystemInfo.MaxSubSlicesSupported, 0u);

    EXPECT_GT_VAL(gtSystemInfo.L3BankCount, 0u);

    EXPECT_TRUE(gtSystemInfo.CCSInfo.IsValid);
    EXPECT_GT_VAL(gtSystemInfo.CCSInfo.NumberOfCCSEnabled, 0u);

    EXPECT_NE_VAL(hwInfoToSet.featureTable.ftrBcsInfo, 0u);
    EXPECT_TRUE(gtSystemInfo.IsDynamicallyPopulated);

    for (uint32_t i = 0; i < gtSystemInfo.SliceCount; i++) {
        EXPECT_TRUE(gtSystemInfo.SliceInfo[i].Enabled);
    }
}

MTLTEST_F(MtlHwInfoTests, givenMtlConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    MtlHwConfig::setupHardwareInfoBase(&hwInfo, false, *compilerProductHelper);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
