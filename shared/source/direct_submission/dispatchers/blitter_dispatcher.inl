/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchPreemption(LinearStream &cmdBuffer) {
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizePreemption() {
    size_t size = 0;
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchMonitorFence(LinearStream &cmdBuffer,
                                                               uint64_t gpuAddress,
                                                               uint64_t immediateData,
                                                               const HardwareInfo &hwInfo) {
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, gpuAddress, immediateData, false, true);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo) {
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, 0ull, 0ull, false, false);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeCacheFlush(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

} // namespace NEO
