/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/unit_test/fixtures/implicit_scaling_fixture.h"

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

TEST_F(ImplicitScalingTests, givenSingleTileApiDisabledWhenOsSupportAndForcedOnThenFeatureEnabled) {
    debugManager.flags.EnableWalkerPartition.set(1);
    ImplicitScaling::apiSupport = false;
    EXPECT_TRUE(ImplicitScalingHelper::isImplicitScalingEnabled(singleTile, false));
}

TEST_F(ImplicitScalingTests, givenMultiTileApiAndOsSupportEnabledWhenForcedOffThenFeatureDisabled) {
    debugManager.flags.EnableWalkerPartition.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingAtomicsForSelfCleanupThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::isAtomicsUsedForSelfCleanup());
}

TEST_F(ImplicitScalingTests, givenForceNotUseAtomicsWhenCheckingAtomicsForSelfCleanupThenExpectFalse) {
    debugManager.flags.UseAtomicsForSelfCleanupSection.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isAtomicsUsedForSelfCleanup());
}

TEST_F(ImplicitScalingTests, givenForceUseAtomicsWhenCheckingAtomicsForSelfCleanupThenExpectTrue) {
    debugManager.flags.UseAtomicsForSelfCleanupSection.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isAtomicsUsedForSelfCleanup());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsIsFalseWhenCheckingProgramSelfCleanupThenExpectFalse) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = true;
    args.synchronizeBeforeExecution = true;
    args.staticPartitioning = false;

    EXPECT_FALSE(ImplicitScalingHelper::isSelfCleanupRequired(args, false));
}

TEST_F(ImplicitScalingTests,
       givenDefaultSettingsAndCrossTileSyncBeforeAndStaticPartititionIsTrueAndCrossTileSyncAfterFalseWhenCheckingProgramSelfCleanupThenExpectTrue) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = false;
    args.synchronizeBeforeExecution = true;
    args.staticPartitioning = true;

    EXPECT_TRUE(ImplicitScalingHelper::isSelfCleanupRequired(args, true));
}

TEST_F(ImplicitScalingTests,
       givenDefaultSettingsAndCrossTileSyncAfterAndStaticPartitionIsTrueAndCrossTileSyncBeforeExecFalseWhenCheckingProgramSelfCleanupThenExpectTrue) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = true;
    args.synchronizeBeforeExecution = false;
    args.staticPartitioning = true;

    EXPECT_TRUE(ImplicitScalingHelper::isSelfCleanupRequired(args, true));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsAndStaticPartititionIsTrueAndAllCrossTileSyncTrueWhenCheckingProgramSelfCleanupThenExpectTrue) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = true;
    args.synchronizeBeforeExecution = true;
    args.staticPartitioning = true;

    EXPECT_TRUE(ImplicitScalingHelper::isSelfCleanupRequired(args, true));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsIsTrueAndStaticPartititionAndAllCrossTileSyncFalseWhenCheckingProgramSelfCleanupThenExpectTrue) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = false;
    args.synchronizeBeforeExecution = false;
    args.staticPartitioning = true;

    EXPECT_FALSE(ImplicitScalingHelper::isSelfCleanupRequired(args, true));
}

TEST_F(ImplicitScalingTests, givenForceNotProgramSelfCleanupWhenDefaultSelfCleanupIsTrueThenExpectFalse) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = true;
    args.synchronizeBeforeExecution = true;
    args.staticPartitioning = false;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isSelfCleanupRequired(args, true));
}

TEST_F(ImplicitScalingTests, givenForceProgramSelfCleanupWhenDefaultSelfCleanupIsFalseThenExpectTrue) {
    WalkerPartition::WalkerPartitionArgs args = {};
    args.crossTileAtomicSynchronization = false;
    args.synchronizeBeforeExecution = false;
    args.staticPartitioning = true;

    debugManager.flags.ProgramWalkerPartitionSelfCleanup.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isSelfCleanupRequired(args, false));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingToProgramWparidRegisterThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::isWparidRegisterInitializationRequired());
}

TEST_F(ImplicitScalingTests, givenForceNotProgramWparidRegisterWhenCheckingRegisterProgramThenExpectFalse) {
    debugManager.flags.WparidRegisterProgramming.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isWparidRegisterInitializationRequired());
}

TEST_F(ImplicitScalingTests, givenForceProgramWparidRegisterWhenCheckingRegisterProgramThenExpectTrue) {
    debugManager.flags.WparidRegisterProgramming.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isWparidRegisterInitializationRequired());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingToUsePipeControlThenExpectTrue) {
    EXPECT_TRUE(ImplicitScalingHelper::isPipeControlStallRequired(true));

    EXPECT_FALSE(ImplicitScalingHelper::isPipeControlStallRequired(false));
}

TEST_F(ImplicitScalingTests, givenForceNotUsePipeControlWhenCheckingPipeControlUseThenExpectFalse) {
    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isPipeControlStallRequired(true));
}

TEST_F(ImplicitScalingTests, givenForceUsePipeControlWhenCheckingPipeControlUseThenExpectTrue) {
    debugManager.flags.UsePipeControlAfterPartitionedWalker.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isPipeControlStallRequired(false));
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingSemaphoreUseThenExpectFalse) {
    EXPECT_FALSE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenForceSemaphoreNotUseWhenCheckingSemaphoreUseThenExpectFalse) {
    debugManager.flags.SynchronizeWithSemaphores.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenForceSemaphoreUseWhenCheckingSemaphoreUseThenExpectTrue) {
    debugManager.flags.SynchronizeWithSemaphores.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isSemaphoreProgrammingRequired());
}

TEST_F(ImplicitScalingTests, givenDefaultSettingsWhenCheckingCrossTileAtomicSyncThenExpectDefaultDefined) {
    EXPECT_FALSE(ImplicitScalingHelper::isCrossTileAtomicRequired(false));

    EXPECT_TRUE(ImplicitScalingHelper::isCrossTileAtomicRequired(true));
}

TEST_F(ImplicitScalingTests, givenForceDisableWhenCheckingCrossTileAtomicSyncThenExpectFalse) {
    debugManager.flags.UseCrossAtomicSynchronization.set(0);
    EXPECT_FALSE(ImplicitScalingHelper::isCrossTileAtomicRequired(true));
}

TEST_F(ImplicitScalingTests, givenForceEnableWhenCheckingCrossTileAtomicSyncThenExpectTrue) {
    debugManager.flags.UseCrossAtomicSynchronization.set(1);
    EXPECT_TRUE(ImplicitScalingHelper::isCrossTileAtomicRequired(false));
}

TEST_F(ImplicitScalingTests, givenMultiTileAndApiSupportOffWhenForcedApiSupportOnThenFeatureEnabled) {
    debugManager.flags.EnableImplicitScaling.set(1);
    ImplicitScaling::apiSupport = false;
    EXPECT_TRUE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}

TEST_F(ImplicitScalingTests, givenMultiTileAndApiSupportOnWhenForcedApiSupportOffThenFeatureDisabled) {
    debugManager.flags.EnableImplicitScaling.set(0);
    ImplicitScaling::apiSupport = true;
    EXPECT_FALSE(ImplicitScalingHelper::isImplicitScalingEnabled(twoTile, true));
}
