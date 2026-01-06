/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"

#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

bool ImplicitScalingHelper::isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition) {
    bool apiSupport = ImplicitScaling::apiSupport;
    int32_t overrideEnableImplicitScaling = debugManager.flags.EnableImplicitScaling.get();
    if (overrideEnableImplicitScaling != -1) {
        apiSupport = !!overrideEnableImplicitScaling;
        preCondition = apiSupport;
    }

    bool partitionWalker = (devices.count() > 1u) &&
                           preCondition &&
                           apiSupport;

    if (debugManager.flags.EnableWalkerPartition.get() != -1) {
        partitionWalker = !!debugManager.flags.EnableWalkerPartition.get();
    }

    return partitionWalker;
}

bool ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired() {
    auto synchronizeBeforeExecution = false;
    int overrideSynchronizeBeforeExecution = debugManager.flags.SynchronizeWalkerInWparidMode.get();
    if (overrideSynchronizeBeforeExecution != -1) {
        synchronizeBeforeExecution = !!overrideSynchronizeBeforeExecution;
    }
    return synchronizeBeforeExecution;
}

bool ImplicitScalingHelper::isSemaphoreProgrammingRequired() {
    auto semaphoreProgrammingRequired = false;
    int overrideSemaphoreProgrammingRequired = debugManager.flags.SynchronizeWithSemaphores.get();
    if (overrideSemaphoreProgrammingRequired != -1) {
        semaphoreProgrammingRequired = !!overrideSemaphoreProgrammingRequired;
    }
    return semaphoreProgrammingRequired;
}

bool ImplicitScalingHelper::isCrossTileAtomicRequired(bool defaultCrossTileRequirement) {
    auto crossTileAtomicSynchronization = defaultCrossTileRequirement;
    int overrideCrossTileAtomicSynchronization = debugManager.flags.UseCrossAtomicSynchronization.get();
    if (overrideCrossTileAtomicSynchronization != -1) {
        crossTileAtomicSynchronization = !!overrideCrossTileAtomicSynchronization;
    }
    return crossTileAtomicSynchronization;
}

bool ImplicitScalingHelper::isAtomicsUsedForSelfCleanup() {
    bool useAtomics = false;
    int overrideUseAtomics = debugManager.flags.UseAtomicsForSelfCleanupSection.get();
    if (overrideUseAtomics != -1) {
        useAtomics = !!(overrideUseAtomics);
    }
    return useAtomics;
}

bool ImplicitScalingHelper::isSelfCleanupRequired(const WalkerPartition::WalkerPartitionArgs &args, bool apiSelfCleanup) {
    bool defaultSelfCleanup = apiSelfCleanup &&
                              (args.crossTileAtomicSynchronization ||
                               args.synchronizeBeforeExecution ||
                               !args.staticPartitioning);

    int overrideProgramSelfCleanup = debugManager.flags.ProgramWalkerPartitionSelfCleanup.get();
    if (overrideProgramSelfCleanup != -1) {
        defaultSelfCleanup = !!(overrideProgramSelfCleanup);
    }
    return defaultSelfCleanup;
}

bool ImplicitScalingHelper::isWparidRegisterInitializationRequired() {
    bool initWparidRegister = false;
    int overrideInitWparidRegister = debugManager.flags.WparidRegisterProgramming.get();
    if (overrideInitWparidRegister != -1) {
        initWparidRegister = !!(overrideInitWparidRegister);
    }
    return initWparidRegister;
}

bool ImplicitScalingHelper::isPipeControlStallRequired(bool defaultEmitPipeControl) {
    int overrideUsePipeControl = debugManager.flags.UsePipeControlAfterPartitionedWalker.get();
    if (overrideUsePipeControl != -1) {
        defaultEmitPipeControl = !!(overrideUsePipeControl);
    }
    return defaultEmitPipeControl;
}

bool ImplicitScalingHelper::pipeControlBeforeCleanupAtomicSyncRequired() {
    int overrideUsePipeControl = debugManager.flags.ProgramStallCommandForSelfCleanup.get();
    if (overrideUsePipeControl != -1) {
        return !!(overrideUsePipeControl);
    }
    return false;
}

} // namespace NEO
