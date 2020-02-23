/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
void BlitterDispatcher<GfxFamily>::dispatchPreemption(LinearStream &cmdBuffer) {
}

template <typename GfxFamily>
size_t BlitterDispatcher<GfxFamily>::getSizePreemption() {
    size_t size = 0;
    return size;
}

template <typename GfxFamily>
void BlitterDispatcher<GfxFamily>::dispatchMonitorFence(LinearStream &cmdBuffer,
                                                        uint64_t gpuAddress,
                                                        uint64_t immediateData,
                                                        const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = 0;
    return size;
}

template <typename GfxFamily>
void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
size_t BlitterDispatcher<GfxFamily>::getSizeCacheFlush(const HardwareInfo &hwInfo) {
    size_t size = 0;
    return size;
}

} // namespace NEO
