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
void CommandListCoreFamily<gfxCoreFamily>::adjustWriteKernelTimestamp(uint64_t globalAddress, uint64_t contextAddress, uint64_t baseAddress, CommandToPatchContainer *outTimeStampSyncCmds,
                                                                      bool workloadPartition, bool copyOperation) {
    uint64_t globalHighAddress = globalAddress + sizeof(uint32_t);
    uint64_t contextHighAddress = contextAddress + sizeof(uint32_t);

    void **globalPostSyncCmdBuffer = nullptr;
    void **contextPostSyncCmdBuffer = nullptr;

    void *globalPostSyncCmd = nullptr;
    void *contextPostSyncCmd = nullptr;

    if (outTimeStampSyncCmds != nullptr) {
        globalPostSyncCmdBuffer = &globalPostSyncCmd;
        contextPostSyncCmdBuffer = &contextPostSyncCmd;
    }

    NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), RegisterOffsets::globalTimestampUn, globalHighAddress, workloadPartition, globalPostSyncCmdBuffer, copyOperation);
    NEO::EncodeStoreMMIO<GfxFamily>::encode(*commandContainer.getCommandStream(), RegisterOffsets::gpThreadTimeRegAddressOffsetHigh, contextHighAddress, workloadPartition, contextPostSyncCmdBuffer, copyOperation);

    if (outTimeStampSyncCmds != nullptr) {
        CommandToPatch ctxCmd;
        ctxCmd.type = CommandToPatch::TimestampEventPostSyncStoreRegMem;

        ctxCmd.offset = globalHighAddress - baseAddress;
        ctxCmd.pDestination = globalPostSyncCmd;
        outTimeStampSyncCmds->push_back(ctxCmd);

        ctxCmd.offset = contextHighAddress - baseAddress;
        ctxCmd.pDestination = contextPostSyncCmd;
        outTimeStampSyncCmds->push_back(ctxCmd);
    }
}

} // namespace L0
