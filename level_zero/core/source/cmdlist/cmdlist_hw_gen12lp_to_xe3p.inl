/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
void CommandListCoreFamily<gfxCoreFamily>::setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, uint64_t forceAggregatedEventIncValue, bool useAdditionalTimestamp) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::kernelMemoryPrefetchEnabled() const { return NEO::debugManager.flags.EnableMemoryPrefetch.get() == 1; }

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::useZeroedPatternInMemoryFill(CmdListMemoryCopyParams &memoryCopyParams) const {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
bool CommandListCoreFamily<gfxCoreFamily>::doParamsRequireCopyOnly(CmdListMemoryCopyParams &memoryCopyParams) const {
    return false;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendCustomOperation(const void *pNext,
                                                                        ze_event_handle_t hSignalEvent,
                                                                        uint32_t numWaitEvents,
                                                                        ze_event_handle_t *phWaitEvents) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
