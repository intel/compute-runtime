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
    DebugManager.flags.UseAtomicsForNativeSectionCleanup.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::useAtomicsForNativeCleanup());
}

TEST_F(ImplicitScalingTests, givenForceUseAtomicsWhenCheckingAtomicsForNativeCleanupThenExpectTrue) {
    DebugManager.flags.UseAtomicsForNativeSectionCleanup.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::useAtomicsForNativeCleanup());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsIsFalseWhenCheckingProgramNativeCleanupThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::programNativeCleanup(false));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsIsTrueWhenCheckingProgramNativeCleanupThenExpectTrue) {
    EXPECT_TRUE(ImplicitScalingHelper::programNativeCleanup(true));
}

TEST_F(ImplicitScalingTests, givenForceNotProgramNativeCleanupWhenDefaultNativeCleanupIsTrueThenExpectFalse) {
    DebugManager.flags.ProgramNativeCleanup.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::programNativeCleanup(true));
}

TEST_F(ImplicitScalingTests, givenForceProgramNativeCleanupWhenDefaultNativeCleanupIsFalseThenExpectTrue) {
    DebugManager.flags.ProgramNativeCleanup.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::programNativeCleanup(false));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingToProgramWparidRegisterThenExpectTrue) {
    EXPECT_TRUE(ImplicitScalingHelper::initWparidRegister());
}

TEST_F(ImplicitScalingTests, givenForceNotProgramWparidRegisterWhenCheckingRegisterProgramThenExpectFalse) {
    DebugManager.flags.WparidRegisterProgramming.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::initWparidRegister());
}

TEST_F(ImplicitScalingTests, givenForceProgramWparidRegisterWhenCheckingRegisterProgramThenExpectTrue) {
    DebugManager.flags.WparidRegisterProgramming.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::initWparidRegister());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingToUsePipeControlThenExpectTrue) {
    EXPECT_TRUE(ImplicitScalingHelper::usePipeControl());
}

TEST_F(ImplicitScalingTests, givenForceNotUsePipeControlWhenCheckingPipeControlUseThenExpectFalse) {
    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::usePipeControl());
}

TEST_F(ImplicitScalingTests, givenForceUsePipeControlWhenCheckingPipeControlUseThenExpectTrue) {
    DebugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::usePipeControl());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingSemaphoreUseThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenForceSemaphoreNotUseWhenCheckingSemaphoreUseThenExpectFalse) {
    DebugManager.flags.SynchronizeWithSemaphores.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenForceSemaphoreUseWhenCheckingSemaphoreUseThenExpectTrue) {
    DebugManager.flags.SynchronizeWithSemaphores.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingCrossTileAtomicSyncThenExpectTrue) {
    EXPECT_TRUE(ImplicitScalingHelper::isCrossTileAtomicRequired());
}

TEST_F(ImplicitScalingTests, givenForceDisableWhenCheckingCrossTileAtomicSyncThenExpectFalse) {
    DebugManager.flags.UseCrossAtomicSynchronization.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isCrossTileAtomicRequired());
}

TEST_F(ImplicitScalingTests, givenForceEnableWhenCheckingCrossTileAtomicSyncThenExpectTrue) {
    DebugManager.flags.UseCrossAtomicSynchronization.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isCrossTileAtomicRequired());
}
