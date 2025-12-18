/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
constexpr bool CommandListCoreFamily<gfxCoreFamily>::checkIfAllocationImportedRequired() {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, uint64_t forceAggregatedEventIncValue, bool useAdditionalTimestamp) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled() const { return NEO::debugManager.flags.EnableMemoryPrefetch.get() == 1; }

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::addPatchScratchAddressInImplicitArgs(CommandsToPatch &commandsToPatch, NEO::EncodeDispatchKernelArgs &args, const NEO::KernelDescriptor &kernelDescriptor, bool kernelNeedsImplicitArgs) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamilyImmediate<gfxCoreFamily>::skipInOrderNonWalkerSignalingAllowed(ze_event_handle_t signalEvent) const {
    if (!NEO::debugManager.flags.SkipInOrderNonWalkerSignalingAllowed.get()) {
        return false;
    }
    return this->isInOrderNonWalkerSignalingRequired(Event::fromHandle(signalEvent));
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::clearCommandsToPatch() {

    auto clearCommandToPatchLambda = [&](auto &patch) {
        using PatchT = std::decay_t<decltype(patch)>;
        if constexpr (NEO::isAnyOfType<PatchT, PatchPauseOnEnqueueSemaphoreStart,
                                       PatchPauseOnEnqueueSemaphoreEnd,
                                       PatchPauseOnEnqueuePipeControlStart,
                                       PatchPauseOnEnqueuePipeControlEnd,
                                       PatchHostFunctionId,
                                       PatchHostFunctionWait>) {
            UNRECOVERABLE_IF(patch.pCommand == nullptr);
        } else if constexpr (std::is_same_v<PatchT, PatchFrontEndState>) {
            using FrontEndStateCommand = typename GfxFamily::FrontEndStateCommand;
            UNRECOVERABLE_IF(patch.pCommand == nullptr);
            delete reinterpret_cast<FrontEndStateCommand *>(patch.pCommand);
        } else if constexpr (NEO::isAnyOfType<PatchT,
                                              PatchComputeWalkerInlineDataScratch,
                                              PatchComputeWalkerImplicitArgsScratch,
                                              PatchNoopSpace>) {
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
}

} // namespace L0
