/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/helpers/definitions/mi_flush_args.h"
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
                                                               bool useNotifyEnable,
                                                               bool partitionedWorkload,
                                                               bool dcFlushRequired) {
    MiFlushArgs args;
    args.commandWithPostSync = true;
    args.notifyEnable = useNotifyEnable;
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, gpuAddress, immediateData, args, productHelper);
}

template <typename GfxFamily>
inline size_t BlitterDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = EncodeMiFlushDW<GfxFamily>::getMiFlushDwCmdSizeForDataWrite();
    return size;
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const RootDeviceEnvironment &rootDeviceEnvironment, uint64_t address) {

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    dispatchTlbFlush(cmdBuffer, address, productHelper);
}

template <typename GfxFamily>
inline void BlitterDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const ProductHelper &productHelper) {
    MiFlushArgs args;
    args.tlbFlush = true;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<GfxFamily>::programMiFlushDw(cmdBuffer, address, 0ull, args, productHelper);
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
