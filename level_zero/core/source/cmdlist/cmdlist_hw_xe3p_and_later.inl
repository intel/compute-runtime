/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/flush_caches_bitmask.h"

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
void CommandListCoreFamily<gfxCoreFamily>::setupFlushL3Flags(bool &isFlushL3ForExternalAllocationRequired, bool &isFlushL3ForHostUsmRequired, bool isFlushL3AfterPostSync, bool isKernelUsingExternalAllocation, bool isKernelUsingSystemAllocation) {
    isFlushL3ForExternalAllocationRequired = isFlushL3AfterPostSync && isKernelUsingExternalAllocation;
    isFlushL3ForHostUsmRequired = isFlushL3AfterPostSync && isKernelUsingSystemAllocation;

    if (NEO::debugManager.flags.RedirectFlushL3HostUsmToExternal.get() && isFlushL3ForHostUsmRequired) {
        isFlushL3ForExternalAllocationRequired = true;
        isFlushL3ForHostUsmRequired = false;
    }

    auto flushCachesMask = NEO::debugManager.flags.FlushAllCaches.get();
    if (flushCachesMask) {
        if (flushCachesMask & NEO::FlushCachesBitmask::l2Flush) {
            isFlushL3ForExternalAllocationRequired = true;
        }
        if (flushCachesMask & NEO::FlushCachesBitmask::l2TransientFlush) {
            isFlushL3ForHostUsmRequired = true;
        }
    }

    if (NEO::debugManager.flags.ForceFlushL3AfterPostSyncForExternalAllocation.get()) {
        isFlushL3ForExternalAllocationRequired = true;
    }
    if (NEO::debugManager.flags.ForceFlushL3AfterPostSyncForHostUsm.get()) {
        isFlushL3ForHostUsmRequired = true;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::addPatchScratchAddressInImplicitArgs(CommandsToPatch &commandsToPatch, NEO::EncodeDispatchKernelArgs &args, const NEO::KernelDescriptor &kernelDescriptor, bool kernelNeedsImplicitArgs) {
    if (kernelNeedsImplicitArgs) {
        auto offset = args.dispatchInterface->getImplicitArgs()->getScratchPtrOffset();
        if (!offset.has_value()) {
            return;
        }

        commandsToPatch.push_back(PatchComputeWalkerImplicitArgsScratch{});
        auto &scratchImplicitArgs =
            std::get<PatchComputeWalkerImplicitArgsScratch>(commandsToPatch[commandsToPatch.size() - 1]);

        scratchImplicitArgs.pDestination = args.outImplicitArgsPtr;
        scratchImplicitArgs.gpuAddress = args.outImplicitArgsGpuVa;
        scratchImplicitArgs.scratchAddressAfterPatch = 0u;

        scratchImplicitArgs.offset = offset.value();
        scratchImplicitArgs.patchSize = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize;
        auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (ssh != nullptr) {
            scratchImplicitArgs.baseAddress = ssh->getGpuBase();
        }
        this->activeScratchPatchElements++;
    }
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::clearCommandsToPatch() {

    auto clearCommandToPatchLambda = [&](auto &patch) {
        using PatchT = std::decay_t<decltype(patch)>;
        if constexpr (NEO::isAnyOfType<PatchT, PatchPauseOnEnqueueSemaphoreStart,
                                       PatchPauseOnEnqueueSemaphoreEnd,
                                       PatchPauseOnEnqueuePipeControlStart,
                                       PatchPauseOnEnqueuePipeControlEnd>) {
            UNRECOVERABLE_IF(patch.pCommand == nullptr);
        } else if constexpr (NEO::isAnyOfType<PatchT,
                                              PatchComputeWalkerInlineDataScratch,
                                              PatchComputeWalkerImplicitArgsScratch,
                                              PatchNoopSpace,
                                              PatchHostFunctionId,
                                              PatchHostFunctionWait>) {
            // nothing to clear

        } else {
            UNRECOVERABLE_IF(true);
        }
    };

    for (auto &commandToPatch : commandsToPatch) {
        std::visit(clearCommandToPatchLambda,
                   commandToPatch);
    }

    commandsToPatch.clear();

    this->frontEndPatchListCount = 0;
    this->activeScratchPatchElements = 0;
    this->hostFunctionPatchListCount = 0;
}

} // namespace L0
