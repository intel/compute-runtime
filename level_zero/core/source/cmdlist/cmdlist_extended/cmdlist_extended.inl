/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"

namespace L0 {

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendFrontEndCopy(NEO::GraphicsAllocation *dstAlloc, size_t dstOffset,
                                                                     NEO::GraphicsAllocation *srcAlloc, size_t srcOffset,
                                                                     size_t size, ze_event_handle_t hSignalEvent,
                                                                     uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents,
                                                                     CmdListMemoryCopyParams &memoryCopyParams) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIBBStart(uint64_t address,
                                                                  size_t predication,
                                                                  bool secondLevel) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMIBBEnd() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendMINoop() {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendPipeControl(void *dstPtr, uint64_t value) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <GFXCORE_FAMILY gfxCoreFamily>
ze_result_t CommandListCoreFamily<gfxCoreFamily>::appendSoftwareTag(const char *data) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

} // namespace L0
