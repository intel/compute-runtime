/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPipeControlExtraProperties(PIPE_CONTROL &pipeControl, PipeControlArgs &args) {
    pipeControl.setHdcPipelineFlush(args.hdcPipelineFlush);
    pipeControl.setUnTypedDataPortCacheFlush(args.unTypedDataPortCacheFlush);
    pipeControl.setCompressionControlSurfaceCcsFlush(args.compressionControlSurfaceCcsFlush);
    pipeControl.setWorkloadPartitionIdOffsetEnable(args.workloadPartitionOffset);
    pipeControl.setAmfsFlushEnable(args.amfsFlushEnable);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pipeControl.setHdcPipelineFlush(true);
        pipeControl.setUnTypedDataPortCacheFlush(true);
        pipeControl.setCompressionControlSurfaceCcsFlush(true);
    }
    if (DebugManager.flags.DoNotFlushCaches.get()) {
        pipeControl.setHdcPipelineFlush(false);
        pipeControl.setUnTypedDataPortCacheFlush(false);
        pipeControl.setCompressionControlSurfaceCcsFlush(false);
    }
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setPostSyncExtraProperties(PipeControlArgs &args, const HardwareInfo &hwInfo) {
    if (hwInfo.featureTable.flags.ftrLocalMemory) {
        args.hdcPipelineFlush = true;
        args.unTypedDataPortCacheFlush = true;
    }
}

template <typename GfxFamily>
inline void MemorySynchronizationCommands<GfxFamily>::setCacheFlushExtraProperties(PipeControlArgs &args) {
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
}

template <typename GfxFamily>
void MemorySynchronizationCommands<GfxFamily>::setPipeControlWAFlags(PIPE_CONTROL &pipeControl) {
    pipeControl.setCommandStreamerStallEnable(true);
    pipeControl.setHdcPipelineFlush(true);
    pipeControl.setUnTypedDataPortCacheFlush(true);
}

template <>
bool HwHelperHw<Family>::unTypedDataPortCacheFlushRequired() const {
    return true;
}

} // namespace NEO
