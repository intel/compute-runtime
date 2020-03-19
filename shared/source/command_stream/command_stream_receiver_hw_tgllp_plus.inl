/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/state_compute_mode_helper.h"

namespace NEO {
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags) {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    if (isComputeModeNeeded()) {
        programAdditionalPipelineSelect(stream, dispatchFlags.pipelineSelectArgs, true);
        auto stateComputeMode = stream.getSpaceForCmd<STATE_COMPUTE_MODE>();
        *stateComputeMode = GfxFamily::cmdInitStateComputeMode;

        FORCE_NON_COHERENT coherencyValue = !dispatchFlags.requiresCoherency ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED;
        stateComputeMode->setForceNonCoherent(coherencyValue);
        this->lastSentCoherencyRequest = static_cast<int8_t>(dispatchFlags.requiresCoherency);

        stateComputeMode->setMaskBits(GfxFamily::stateComputeModeForceNonCoherentMask);

        if (csrSizeRequestFlags.hasSharedHandles) {
            auto pc = stream.getSpaceForCmd<PIPE_CONTROL>();
            *pc = GfxFamily::cmdInitPipeControl;
        }
        adjustComputeMode(stream, dispatchFlags, stateComputeMode);
        programAdditionalPipelineSelect(stream, dispatchFlags.pipelineSelectArgs, false);
    }
}

template <>
inline bool CommandStreamReceiverHw<Family>::isComputeModeNeeded() const {
    return csrSizeRequestFlags.coherencyRequestChanged || csrSizeRequestFlags.hasSharedHandles || csrSizeRequestFlags.numGrfRequiredChanged ||
           StateComputeModeHelper<Family>::isStateComputeModeRequired(csrSizeRequestFlags, this->lastSentThreadArbitrationPolicy != this->requiredThreadArbitrationPolicy);
}

template <>
inline typename Family::PIPE_CONTROL *CommandStreamReceiverHw<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream) {
    auto pCmd = addPipeControlCmd(commandStream);
    pCmd->setTextureCacheInvalidationEnable(true);
    pCmd->setDcFlushEnable(true);
    pCmd->setHdcPipelineFlush(true);
    return pCmd;
}
} // namespace NEO
