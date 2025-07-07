/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/kernel/kernel.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
inline NEO::PreemptionMode CommandListCoreFamily<gfxCoreFamily>::obtainKernelPreemptionMode(Kernel *kernel) {
    auto kernelDescriptor = &kernel->getImmutableData()->getDescriptor();
    bool disabledMidThreadPreemptionKernel = kernelDescriptor->kernelAttributes.flags.requiresDisabledMidThreadPreemption;
    bool debuggingEnabled = device->getNEODevice()->getDebugger();
    disabledMidThreadPreemptionKernel |= debuggingEnabled;
    disabledMidThreadPreemptionKernel |= device->getNEODevice()->getPreemptionMode() != NEO::PreemptionMode::MidThread;
    return disabledMidThreadPreemptionKernel ? NEO::PreemptionMode::ThreadGroup : NEO::PreemptionMode::MidThread;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::adjustWriteKernelTimestamp(uint64_t address, uint64_t baseAddress, CommandToPatchContainer *outTimeStampSyncCmds,
                                                                      bool workloadPartition, bool copyOperation, bool globalTimestamp) {
    uint64_t highAddress = address + sizeof(uint32_t);
    void **postSyncCmdBuffer = nullptr;
    void *postSyncCmd = nullptr;

    if (outTimeStampSyncCmds != nullptr) {
        postSyncCmdBuffer = &postSyncCmd;
    }

    uint32_t registerOffset = globalTimestamp ? RegisterOffsets::globalTimestampUn : RegisterOffsets::gpThreadTimeRegAddressOffsetHigh;
    writeTimestamp(commandContainer, registerOffset, highAddress, false, workloadPartition, postSyncCmdBuffer, copyOperation);
    pushTimestampPatch(outTimeStampSyncCmds, highAddress - baseAddress, postSyncCmd);
}
} // namespace L0
