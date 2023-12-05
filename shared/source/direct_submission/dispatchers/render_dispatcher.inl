/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchPreemption(LinearStream &cmdBuffer) {
    PreemptionHelper::programCmdStream<GfxFamily>(cmdBuffer, PreemptionMode::MidBatch, PreemptionMode::Disabled, nullptr);
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizePreemption() {
    size_t size = PreemptionHelper::getRequiredCmdStreamSize<GfxFamily>(PreemptionMode::MidBatch, PreemptionMode::Disabled);
    return size;
}

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchMonitorFence(LinearStream &cmdBuffer,
                                                              uint64_t gpuAddress,
                                                              uint64_t immediateData,
                                                              const RootDeviceEnvironment &rootDeviceEnvironment,
                                                              bool useNotifyEnable,
                                                              bool partitionedWorkload,
                                                              bool dcFlushRequired) {
    PipeControlArgs args;
    args.dcFlushEnable = dcFlushRequired;
    args.workloadPartitionOffset = partitionedWorkload;
    args.notifyEnable = useNotifyEnable;
    args.textureCacheInvalidationEnable = true;

    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        cmdBuffer,
        PostSyncMode::immediateData,
        gpuAddress,
        immediateData,
        rootDeviceEnvironment,
        args);
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeMonitorFence(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, false);
}

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const RootDeviceEnvironment &rootDeviceEnvironment, uint64_t address) {
    MemorySynchronizationCommands<GfxFamily>::addFullCacheFlush(cmdBuffer, rootDeviceEnvironment);
}

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address, const RootDeviceEnvironment &rootDeviceEnvironment) {
    PipeControlArgs args;
    args.tlbInvalidation = true;
    args.pipeControlFlushEnable = true;
    args.textureCacheInvalidationEnable = true;

    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdBuffer, args);
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeCacheFlush(const RootDeviceEnvironment &rootDeviceEnvironment) {
    size_t size = MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush();
    return size;
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeTlbFlush(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier(true);
}

} // namespace NEO
