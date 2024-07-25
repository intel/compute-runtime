/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/test.h"
using namespace NEO;

TEST(HwInfoTest, givenReleaseHelperWithL3BankConfigWhenSetupDefaultHwInfoThenL3ConfigIsProperlySet) {
    HardwareInfo hwInfo{};
    MockReleaseHelper releaseHelper;

    releaseHelper.getL3BankCountResult = 0;
    releaseHelper.getL3CacheBankSizeInKbResult = 0;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(0u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 0;
    releaseHelper.getL3CacheBankSizeInKbResult = 4;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(0u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(0u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 2;
    releaseHelper.getL3CacheBankSizeInKbResult = 0;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(2u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(1u, hwInfo.gtSystemInfo.L3CacheSizeInKb);

    releaseHelper.getL3BankCountResult = 3;
    releaseHelper.getL3CacheBankSizeInKbResult = 3;

    setupDefaultGtSysInfo(&hwInfo, &releaseHelper);

    EXPECT_EQ_VAL(3u, hwInfo.gtSystemInfo.L3BankCount);
    EXPECT_EQ_VAL(9u, hwInfo.gtSystemInfo.L3CacheSizeInKb);
}

TEST(HwInfoTest, whenSettingDefaultFeatureTableAndWorkaroundTableThenProperFieldsAreSet) {
    HardwareInfo hwInfo{};
    FeatureTable expectedFeatureTable{};
    WorkaroundTable expectedWorkaroundTable{};

    expectedFeatureTable.flags.ftrAstcHdr2D = true;
    expectedFeatureTable.flags.ftrAstcLdr2D = true;
    expectedFeatureTable.flags.ftrCCSNode = true;
    expectedFeatureTable.flags.ftrCCSRing = true;
    expectedFeatureTable.flags.ftrFbc = true;
    expectedFeatureTable.flags.ftrGpGpuMidBatchPreempt = true;
    expectedFeatureTable.flags.ftrGpGpuThreadGroupLevelPreempt = true;
    expectedFeatureTable.flags.ftrIA32eGfxPTEs = true;
    expectedFeatureTable.flags.ftrL3IACoherency = true;
    expectedFeatureTable.flags.ftrLinearCCS = true;
    expectedFeatureTable.flags.ftrPPGTT = true;
    expectedFeatureTable.flags.ftrSVM = true;
    expectedFeatureTable.flags.ftrStandardMipTailFormat = true;
    expectedFeatureTable.flags.ftrTileMappedResource = true;
    expectedFeatureTable.flags.ftrTranslationTable = true;
    expectedFeatureTable.flags.ftrUserModeTranslationTable = true;

    expectedWorkaroundTable.flags.wa4kAlignUVOffsetNV12LinearSurface = true;

    setupDefaultFeatureTableAndWorkaroundTable(&hwInfo);

    EXPECT_EQ(expectedFeatureTable.asHash(), hwInfo.featureTable.asHash());
    EXPECT_EQ(expectedWorkaroundTable.asHash(), hwInfo.workaroundTable.asHash());
}
