/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/state_compute_mode_helper.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags, const HardwareInfo &hwInfo) {
    if (isComputeModeNeeded()) {
        EncodeComputeMode<GfxFamily>::programComputeModeCommandWithSynchronization(
            stream, this->streamProperties.stateComputeMode, dispatchFlags.pipelineSelectArgs,
            hasSharedHandles(), hwInfo, isRcs());
    }
}

template <typename GfxFamily>
inline bool CommandStreamReceiverHw<GfxFamily>::isComputeModeNeeded() const {
    return StateComputeModeHelper<Family>::isStateComputeModeRequired(csrSizeRequestFlags, false) ||
           this->streamProperties.stateComputeMode.isDirty();
}

template <>
inline void CommandStreamReceiverHw<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<Family>::getDcFlushEnable(true, peekHwInfo());
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, peekHwInfo(), isRcs());
}

} // namespace NEO
