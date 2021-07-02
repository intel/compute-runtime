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
    if (DebugManager.flags.SynchronizeWalkerInWparidMode.get() != -1) {
        synchronizeBeforeExecution = static_cast<bool>(DebugManager.flags.SynchronizeWalkerInWparidMode.get());
    }
    return synchronizeBeforeExecution;
}

bool ImplicitScalingHelper::useAtomicsForNativeCleanup() {
    bool useAtomics = false;
    int overrideUseAtomics = DebugManager.flags.ExperimentalUseAtomicsForNativeSectionCleanup.get();
    if (overrideUseAtomics != -1) {
        useAtomics = !!(overrideUseAtomics);
    }
    return useAtomics;
}
} // namespace NEO
