/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

#include "implicit_args.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::skipInOrderNonWalkerSignalingAllowed(ze_event_handle_t signalEvent) const {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
constexpr bool CommandListCoreFamily<gfxCoreFamily>::checkIfAllocationImportedRequired() {
    return true;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::addPatchScratchAddressInImplicitArgs(CommandsToPatch &commandsToPatch, NEO::EncodeDispatchKernelArgs &args, const NEO::KernelDescriptor &kernelDescriptor, bool kernelNeedsImplicitArgs) {
    if (kernelNeedsImplicitArgs) {
        auto offset = args.dispatchInterface->getImplicitArgs()->getScratchPtrOffset();
        if (!offset.has_value()) {
            return;
        }
        CommandToPatch scratchImplicitArgs;

        scratchImplicitArgs.pDestination = args.outImplicitArgsPtr;
        scratchImplicitArgs.gpuAddress = args.outImplicitArgsGpuVa;
        scratchImplicitArgs.scratchAddressAfterPatch = 0u;
        scratchImplicitArgs.type = CommandToPatch::CommandType::ComputeWalkerImplicitArgsScratch;
        scratchImplicitArgs.offset = offset.value();
        scratchImplicitArgs.patchSize = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize;
        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (ssh != nullptr) {
            scratchImplicitArgs.baseAddress = ssh->getGpuBase();
        }
        commandsToPatch.push_back(scratchImplicitArgs);
        this->activeScratchPatchElements++;
    }
}

} // namespace L0
