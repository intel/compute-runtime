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

MTLTEST_F(MtlHwInfoTests, WhenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValueOfFtrLinearCCS) {
    HardwareInfo hwInfo = *defaultHwInfo;
    auto compilerProductHelper = CompilerProductHelper::create(hwInfo.platform.eProductFamily);
    FeatureTable &featureTable = hwInfo.featureTable;

    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    MtlHwConfig::setupHardwareInfo(&hwInfo, false, *compilerProductHelper);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    MtlHwConfig::setupHardwareInfo(&hwInfo, true, *compilerProductHelper);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
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
