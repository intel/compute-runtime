/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_release_helper.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

TEST(HwInfoTest, whenSettingDefaultFeatureTableAndWorkaroundTableThenProperFieldsAreSet) {
    HardwareInfo hwInfo{};
    FeatureTable expectedFeatureTable{};
    WorkaroundTable expectedWorkaroundTable{};
    MockReleaseHelper mockReleaseHelper;

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

    setupDefaultFeatureTableAndWorkaroundTable(&hwInfo, mockReleaseHelper);

    EXPECT_EQ(expectedFeatureTable.asHash(), hwInfo.featureTable.asHash());
    EXPECT_EQ(expectedWorkaroundTable.asHash(), hwInfo.workaroundTable.asHash());
}

TEST(HwInfoTest, givenHwInfoWhenQueryNumSubSlicesPerSliceThenCorrectNumberIsReturned) {
    HardwareInfo hwInfo{};
    hwInfo.gtSystemInfo.SliceCount = 2;
    hwInfo.gtSystemInfo.SubSliceCount = 7;
    uint32_t expectedNumSubSlicesPerSlice = 4;
    EXPECT_EQ(getNumSubSlicesPerSlice(hwInfo), expectedNumSubSlicesPerSlice);
}

TEST(HwInfoTest, whenGettingHwInfoForUnspecifiedPlatformStringThenSuccessIsReturned) {
    HardwareInfo hwInfo{};
    const HardwareInfo *pHwInfo = &hwInfo;
    std::string unspecifiedPlatform = debugManager.flags.ProductFamilyOverride.get();
    EXPECT_TRUE(getHwInfoForPlatformString(unspecifiedPlatform, pHwInfo));
    EXPECT_EQ(pHwInfo, &hwInfo);
}

TEST(HwInfoTest, whenGettingHwInfoForUnknownPlatformStringThenFailureIsReturned) {
    HardwareInfo hwInfo{};
    const HardwareInfo *pHwInfo = &hwInfo;
    std::string invalidPlatformString = "invalid";
    EXPECT_FALSE(getHwInfoForPlatformString(invalidPlatformString, pHwInfo));
}

TEST(HwInfoTest, whenApplyDebugOverrideCalledThenDebugVariablesAreApplied) {
    DebugManagerStateRestore debugRestorer;
    debugManager.flags.OverrideNumThreadsPerEu.set(123);

    HardwareInfo hwInfo{};
    hwInfo.gtSystemInfo.EUCount = 2;

    applyDebugOverrides(hwInfo);
    EXPECT_EQ(hwInfo.gtSystemInfo.NumThreadsPerEu, 123u);
    EXPECT_EQ(hwInfo.gtSystemInfo.ThreadCount, hwInfo.gtSystemInfo.EUCount * 123u);
}
