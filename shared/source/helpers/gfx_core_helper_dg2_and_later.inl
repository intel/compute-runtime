/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setBarrierExtraProperties(void *barrierCmd, PipeControlArgs &args) {
    auto &pipeControl = *reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);
    pipeControl.setUnTypedDataPortCacheFlush(args.unTypedDataPortCacheFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);

    if (debugManager.flags.FlushAllCaches.get()) {
        pipeControl.setHdcPipelineFlush(true);
        pipeControl.setUnTypedDataPortCacheFlush(true);
        pipeControl.setCompressionControlSurfaceCcsFlush(true);
    }
    if (debugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setHdcPipelineFlush(false);
        pipeControl.setUnTypedDataPortCacheFlush(false);
        pipeControl.setCompressionControlSurfaceCcsFlush(false);
    }
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setBarrierWaFlags(void *barrierCmd) {
    auto &pipeControl = *reinterpret_cast<typename GfxFamily::PIPE_CONTROL *>(barrierCmd);

    pipeControl.setCommandStreamerStallEnable(true);
    pipeControl.setHdcPipelineFlush(true);
    pipeControl.setUnTypedDataPortCacheFlush(true);
}

template <>
bool GfxCoreHelperHw<Family>::isUpdateTaskCountFromWaitSupported() const {
    return true;
}

template <>
bool GfxCoreHelperHw<Family>::unTypedDataPortCacheFlushRequired() const {
    return true;
}
} // namespace NEO
