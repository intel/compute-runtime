/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/implicit_scaling_fixture.h"

TEST_F(ImplicitScalingTests, givenMultiTileDeviceWhenApiAndOsSupportThenFeatureEnabled) {
    EXPECT_TRUE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenSingleTileDeviceWhenApiAndOsSupportThenFeatureDisabled) {
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(singleTile, true));
}

TEST_F(ImplicitScalingTests, givenMultiTileAndPreconditionFalseWhenApiAndOsSupportThenFeatureDisabled) {
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, false));
}

TEST_F(ImplicitScalingTests, givenMultiTileAndOsSupportWhenApiDisabledThenFeatureDisabled) {
    ImplicitScaling::apiSupport = false;
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenMultiTileAndApiSupportWhenOsDisabledThenFeatureDisabled) {
    OSInterface::osEnableLocalMemory = false;
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenSingleTileApiDisabledWhenOsSupportAndForcedOnThenFeatureEnabled) {
    DebugManager.flags.EnableWalkerPartition.set(1);
    ImplicitScaling::apiSupport = false;
    EXPECT_TRUE(ImplicitScalingHelper::isImplicitScalingEnabled(singleTile, false));
}

TEST_F(ImplicitScalingTests, givenMultiTileApiAndOsSupportEnabledWhenForcedOffThenFeatureDisabled) {
    DebugManager.flags.EnableWalkerPartition.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenMultiTileApiEnabledWhenOsSupportOffAndForcedOnThenFeatureDisabled) {
    DebugManager.flags.EnableWalkerPartition.set(1);
    OSInterface::osEnableLocalMemory = false;
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingAtomicsForNativeCleanupThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::useAtomicsForNativeCleanup());
}

TEST_F(ImplicitScalingTests, givenForceNotUseAtomicsWhenCheckingAtomicsForNativeCleanupThenExpectFalse) {
    DebugManager.flags.ExperimentalUseAtomicsForNativeSectionCleanup.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::useAtomicsForNativeCleanup());
}

TEST_F(ImplicitScalingTests, givenForceUseAtomicsWhenCheckingAtomicsForNativeCleanupThenExpectTrue) {
    DebugManager.flags.ExperimentalUseAtomicsForNativeSectionCleanup.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::useAtomicsForNativeCleanup());
}
