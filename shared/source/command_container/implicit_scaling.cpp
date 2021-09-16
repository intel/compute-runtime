/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

bool ImplicitScalingHelper::isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition) {
    bool partitionWalker = (devices.count() > 1u) &&
                           preCondition &&
                           ImplicitScaling::apiSupport;

    if (DebugManager.flags.EnableWalkerPartition.get() != -1) {
        partitionWalker = !!DebugManager.flags.EnableWalkerPartition.get();
    }
    //we can't do this without local memory
    partitionWalker &= OSInterface::osEnableLocalMemory;

    return partitionWalker;
}

bool ImplicitScalingHelper::isSynchronizeBeforeExecutionRequired() {
    auto synchronizeBeforeExecution = false;
    int overrideSynchronizeBeforeExecution = DebugManager.flags.SynchronizeWalkerInWparidMode.get();
    if (overrideSynchronizeBeforeExecution != -1) {
        synchronizeBeforeExecution = !!overrideSynchronizeBeforeExecution;
    }
    return synchronizeBeforeExecution;
}

bool ImplicitScalingHelper::isSemaphoreProgrammingRequired() {
    auto semaphoreProgrammingRequired = ImplicitScaling::semaphoreProgrammingRequired;
    int overrideSemaphoreProgrammingRequired = NEO::DebugManager.flags.SynchronizeWithSemaphores.get();
    if (overrideSemaphoreProgrammingRequired != -1) {
        semaphoreProgrammingRequired = !!overrideSemaphoreProgrammingRequired;
    }
    return semaphoreProgrammingRequired;
}

bool ImplicitScalingHelper::isCrossTileAtomicRequired() {
    auto crossTileAtomicSynchronization = ImplicitScaling::crossTileAtomicSynchronization;
    int overrideCrossTileAtomicSynchronization = NEO::DebugManager.flags.UseCrossAtomicSynchronization.get();
    if (overrideCrossTileAtomicSynchronization != -1) {
        crossTileAtomicSynchronization = !!overrideCrossTileAtomicSynchronization;
    }
    return crossTileAtomicSynchronization;
}

bool ImplicitScalingHelper::isAtomicsUsedForSelfCleanup() {
    bool useAtomics = false;
    int overrideUseAtomics = DebugManager.flags.UseAtomicsForSelfCleanupSection.get();
    if (overrideUseAtomics != -1) {
        useAtomics = !!(overrideUseAtomics);
    }
    return useAtomics;
}

bool ImplicitScalingHelper::isSelfCleanupRequired(bool defaultSelfCleanup) {
    int overrideProgramSelfCleanup = DebugManager.flags.ProgramWalkerPartitionSelfCleanup.get();
    if (overrideProgramSelfCleanup != -1) {
        defaultSelfCleanup = !!(overrideProgramSelfCleanup);
    }
    return defaultSelfCleanup;
}

bool ImplicitScalingHelper::isWparidRegisterInitializationRequired() {
    bool initWparidRegister = true;
    int overrideInitWparidRegister = DebugManager.flags.WparidRegisterProgramming.get();
    if (overrideInitWparidRegister != -1) {
        initWparidRegister = !!(overrideInitWparidRegister);
    }
    return initWparidRegister;
}

bool ImplicitScalingHelper::isPipeControlStallRequired() {
    bool emitPipeControl = true;
    int overrideUsePipeControl = DebugManager.flags.UsePipeControlAfterPartitionedWalker.get();
    if (overrideUsePipeControl != -1) {
        emitPipeControl = !!(overrideUsePipeControl);
    }
    return emitPipeControl;
}

} // namespace NEO
