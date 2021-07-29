/*
 * Copyright (C) 2020-2021 Intel Corporation
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
                                                               const HardwareInfo &hwInfo,
                                                               bool useNotifyEnable) {
    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = useNotifyEnable;
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, gpuAddress, immediateData, args);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo, uint64_t address) {
    dispatchTlbFlush(cmdBuffer, address);
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address) {
    MiFlushArgs args;
    args.tlbFlush = true;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, address, 0ull, args);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeCacheFlush(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeTlbFlush() {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

} // namespace NEO
