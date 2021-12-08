/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/state_compute_mode_helper.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "pipe_control_args.h"

namespace NEO {
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags, const HardwareInfo &hwInfo) {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    if (isComputeModeNeeded()) {
        programAdditionalPipelineSelect(stream, dispatchFlags.pipelineSelectArgs, true);
        this->lastSentCoherencyRequest = static_cast<int8_t>(dispatchFlags.requiresCoherency);

        auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
        if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs())) {
            PipeControlArgs args(true);

            addPipeControlPriorToNonPipelinedStateCommand(stream, args);
        }

        StreamProperties properties{};
        properties.stateComputeMode.setProperties(dispatchFlags.requiresCoherency, dispatchFlags.numGrfRequired,
                                                  this->requiredThreadArbitrationPolicy);
        EncodeComputeMode<GfxFamily>::programComputeModeCommand(stream, properties.stateComputeMode, hwInfo);

        if (csrSizeRequestFlags.hasSharedHandles) {
            auto pc = stream.getSpaceForCmd<PIPE_CONTROL>();
            *pc = GfxFamily::cmdInitPipeControl;
        }
        programAdditionalPipelineSelect(stream, dispatchFlags.pipelineSelectArgs, false);
    }
}

template <>
inline bool CommandStreamReceiverHw<Family>::isComputeModeNeeded() const {
    return StateComputeModeHelper<Family>::isStateComputeModeRequired(csrSizeRequestFlags, this->lastSentThreadArbitrationPolicy != this->requiredThreadArbitrationPolicy);
}

template <>
inline void CommandStreamReceiverHw<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream) {
    PipeControlArgs args(true);
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    addPipeControlPriorToNonPipelinedStateCommand(commandStream, args);
}

} // namespace NEO
