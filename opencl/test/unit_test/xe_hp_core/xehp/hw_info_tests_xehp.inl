/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"

#include "opencl/test/unit_test/helpers/gtest_helpers.h"
#include "test.h"
using namespace NEO;

using XeHPHwInfoTest = ::testing::Test;

XEHPTEST_F(XeHPHwInfoTest, whenSetupHardwareInfoWithSetupFeatureTableFlagTrueOrFalseIsCalledThenFeatureTableHasCorrectValues) {
    HardwareInfo hwInfo = *defaultHwInfo;
    FeatureTable &featureTable = hwInfo.featureTable;
    WorkaroundTable &workaroundTable = hwInfo.workaroundTable;
    GT_SYSTEM_INFO &gtSystemInfo = hwInfo.gtSystemInfo;

    EXPECT_FALSE(featureTable.ftrLocalMemory);
    EXPECT_FALSE(featureTable.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.ftrLinearCCS);
    EXPECT_FALSE(featureTable.ftrE2ECompression);
    EXPECT_FALSE(featureTable.ftrCCSNode);
    EXPECT_FALSE(featureTable.ftrCCSRing);
    EXPECT_FALSE(featureTable.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.ftrCCSMultiInstance);
    EXPECT_FALSE(featureTable.ftrLinearCCS);
    EXPECT_FALSE(workaroundTable.waDefaultTile4);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);

    XE_HP_SDV_CONFIG::setupHardwareInfo(&hwInfo, false);
    EXPECT_FALSE(featureTable.ftrLocalMemory);
    EXPECT_FALSE(featureTable.ftrFlatPhysCCS);
    EXPECT_FALSE(featureTable.ftrLinearCCS);
    EXPECT_FALSE(featureTable.ftrE2ECompression);
    EXPECT_FALSE(featureTable.ftrCCSNode);
    EXPECT_FALSE(featureTable.ftrCCSRing);
    EXPECT_FALSE(featureTable.ftrMultiTileArch);
    EXPECT_FALSE(featureTable.ftrCCSMultiInstance);
    EXPECT_FALSE(featureTable.ftrLinearCCS);
    EXPECT_FALSE(workaroundTable.waDefaultTile4);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);

    XE_HP_SDV_CONFIG::setupHardwareInfo(&hwInfo, true);
    EXPECT_TRUE(featureTable.ftrLocalMemory);
    EXPECT_TRUE(featureTable.ftrFlatPhysCCS);
    EXPECT_TRUE(featureTable.ftrLinearCCS);
    EXPECT_TRUE(featureTable.ftrE2ECompression);
    EXPECT_TRUE(featureTable.ftrCCSNode);
    EXPECT_TRUE(featureTable.ftrCCSRing);
    EXPECT_TRUE(featureTable.ftrMultiTileArch);
    EXPECT_TRUE(featureTable.ftrCCSMultiInstance);
    EXPECT_TRUE(featureTable.ftrLinearCCS);
    EXPECT_FALSE(workaroundTable.waDefaultTile4);
    EXPECT_FALSE(gtSystemInfo.IsL3HashModeEnabled);
    EXPECT_FALSE(gtSystemInfo.IsDynamicallyPopulated);
    EXPECT_EQ(8u, gtSystemInfo.CsrSizeInMb);
}

XEHPTEST_F(XeHPHwInfoTest, givenAlreadyInitializedHwInfoWhenSetupCalledThenDontOverride) {
    HardwareInfo hwInfo = *defaultHwInfo;

    hwInfo.gtSystemInfo.SliceCount = 0;

    XE_HP_SDV_CONFIG::setupHardwareInfo(&hwInfo, false);

    EXPECT_NE(0u, hwInfo.gtSystemInfo.SliceCount);

    auto expectedValue = ++hwInfo.gtSystemInfo.SliceCount;

    XE_HP_SDV_CONFIG::setupHardwareInfo(&hwInfo, false);

    EXPECT_EQ(expectedValue, hwInfo.gtSystemInfo.SliceCount);
}