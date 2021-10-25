/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/helpers/hw_helper.h"

#include "pipe_control_args.h"

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
                                                              const HardwareInfo &hwInfo,
                                                              bool useNotifyEnable,
                                                              bool partitionedWorkload) {
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    PipeControlArgs args(true);
    args.workloadPartitionOffset = partitionedWorkload;
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        cmdBuffer,
        POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        gpuAddress,
        immediateData,
        hwInfo,
        args);
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeMonitorFence(const HardwareInfo &hwInfo) {
    size_t size = MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(hwInfo);
    return size;
}

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchCacheFlush(LinearStream &cmdBuffer, const HardwareInfo &hwInfo, uint64_t address) {
    MemorySynchronizationCommands<GfxFamily>::addFullCacheFlush(cmdBuffer);
}

template <typename GfxFamily>
inline void RenderDispatcher<GfxFamily>::dispatchTlbFlush(LinearStream &cmdBuffer, uint64_t address) {
    PipeControlArgs args(false);
    args.tlbInvalidation = true;
    args.pipeControlFlushEnable = true;
    args.textureCacheInvalidationEnable = true;

    MemorySynchronizationCommands<GfxFamily>::addPipeControl(cmdBuffer, args);
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeCacheFlush(const HardwareInfo &hwInfo) {
    size_t size = MemorySynchronizationCommands<GfxFamily>::getSizeForFullCacheFlush();
    return size;
}

template <typename GfxFamily>
inline size_t RenderDispatcher<GfxFamily>::getSizeTlbFlush() {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl();
}

} // namespace NEO
