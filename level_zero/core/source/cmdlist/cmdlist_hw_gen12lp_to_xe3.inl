/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
constexpr bool CommandListCoreFamily<gfxCoreFamily>::checkIfAllocationImportedRequired() {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, bool useAdditionalTimestamp) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled() const { return NEO::debugManager.flags.EnableMemoryPrefetch.get() == 1; }

} // namespace L0
