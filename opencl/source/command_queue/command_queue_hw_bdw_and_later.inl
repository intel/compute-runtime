/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_queue/command_queue_hw_base.inl"

namespace NEO {

template <typename GfxFamily>
void CommandQueueHw<GfxFamily>::submitCacheFlush(Surface **surfaces,
                                                 size_t numSurfaces,
                                                 LinearStream *commandStream,
                                                 uint64_t postSyncAddress) {
}

template <typename GfxFamily>
bool CommandQueueHw<GfxFamily>::isCacheFlushCommand(uint32_t commandType) const {
    return false;
}

} // namespace NEO
