/*
 * Copyright (C) 2020-2022 Intel Corporation
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
                                                               bool useNotifyEnable,
                                                               bool partitionedWorkload,
                                                               bool dcFlushRequired) {
    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = useNotifyEnable;
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, gpuAddress, immediateData, args, hwInfo);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo, uint64_t address) {
    dispatchTlbFlush(cmdBuffer, address, hwInfo);
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const HardwareInfo &hwInfo) {
    MiFlushArgs args;
    args.tlbFlush = true;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, address, 0ull, args, hwInfo);
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
