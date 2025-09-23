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

} // namespace L0
