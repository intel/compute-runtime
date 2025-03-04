/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
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
                                                               const RootDeviceEnvironment &rootDeviceEnvironment,
                                                               bool partitionedWorkload,
                                                               bool dcFlushRequired,
                                                               bool notifyKmd) {
    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    args.notifyEnable = notifyKmd;

    EncodeMiFlushDW<GfxFamily>::programWithWa(cmdBuffer, gpuAddress, immediateData, args);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const RootDeviceEnvironment &rootDeviceEnvironment) {
    EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    size_t size = EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const RootDeviceEnvironment &rootDeviceEnvironment, uint64_t address) {
    dispatchTlbFlush(cmdBuffer, address, rootDeviceEnvironment);
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const RootDeviceEnvironment &rootDeviceEnvironment) {
    NEO::EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    MiFlushArgs args{waArgs};
    args.tlbFlush = true;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<GfxFamily>::programWithWa(cmdBuffer, address, 0ull, args);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeCacheFlush(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return getSizeTlbFlush(rootDeviceEnvironment);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeTlbFlush(const RootDeviceEnvironment &rootDeviceEnvironment) {
    EncodeDummyBlitWaArgs waArgs{false, const_cast<RootDeviceEnvironment *>(&rootDeviceEnvironment)};
    size_t size = EncodeMiFlushDW<GfxFamily>::getCommandSizeWithWa(waArgs);
    return size;
}

} // namespace NEO
