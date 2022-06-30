/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/gtest_helpers.h"

using namespace NEO;

using XeHPHwInfoTest = ::testing::Test;

XEHPTEST_F(XeHPHwInfoTest, whenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.flags.ftrCCSMultiInstance);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);

    XehpSdvHwConfig::setupHardwareInfo(&hwInfo, false);
    EXPECT_FALSE(featureTable.flags.ftrLocalMemory);
    EXPECT_FALSE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(featureTable.flags.ftrE2ECompression);
    EXPECT_FALSE(featureTable.flags.ftrCCSNode);
    EXPECT_FALSE(featureTable.flags.ftrCCSRing);
    EXPECT_FALSE(featureTable.flags.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.flags.ftrCCSMultiInstance);
    EXPECT_FALSE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);

    XehpSdvHwConfig::setupHardwareInfo(&hwInfo, true);
    EXPECT_TRUE(featureTable.flags.ftrLocalMemory);
    EXPECT_TRUE(featureTable.flags.ftrFlatPhysCCS);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
    EXPECT_TRUE(featureTable.flags.ftrE2ECompression);
    EXPECT_TRUE(featureTable.flags.ftrCCSNode);
    EXPECT_TRUE(featureTable.flags.ftrCCSRing);
    EXPECT_TRUE(featureTable.flags.ftrMultiTileArch);
    EXPECT_TRUE(featureTable.flags.ftrCCSMultiInstance);
    EXPECT_TRUE(featureTable.flags.ftrLinearCCS);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
}

XEHPTEST_F(XeHPHwInfoTest, givenAlreadyInitializedHwInfoWhenSetupCalledThenDontOverride) {
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.SliceCount = 0;

    XehpSdvHwConfig::setupHardwareInfo(&hwInfo, false);

    EXPECT_NE(0u, hwInfo.gtSystemInfo.SliceCount);

    auto expectedValue = ++hwInfo.gtSystemInfo.SliceCount;

    XehpSdvHwConfig::setupHardwareInfo(&hwInfo, false);

    EXPECT_EQ(expectedValue, hwInfo.gtSystemInfo.SliceCount);
}

XEHPTEST_F(XeHPHwInfoTest, givenXeHpConfigWhenSetupHardwareInfoBaseThenGtSystemInfoIsCorrect) {
    HardwareInfo hwInfo = *defaultHwInfo;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;
    XE_HP_SDV::setupHardwareInfoBase(&hwInfo, false);

    EXPECT_EQ(336u, gtSystemInfo.TotalVsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalHsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalDsThreads);
    EXPECT_EQ(336u, gtSystemInfo.TotalGsThreads);
    EXPECT_EQ(64u, gtSystemInfo.TotalPsThreadsWindowerRange);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
}
