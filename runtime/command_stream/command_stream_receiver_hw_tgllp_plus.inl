/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"

namespace NEO {
template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeMode(LinearStream &stream, DispatchFlags &dispatchFlags) {
    using STATE_COMPUTE_MODE = typename GfxFamily::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    if (csrSizeRequestFlags.coherencyRequestChanged || csrSizeRequestFlags.hasSharedHandles || csrSizeRequestFlags.numGrfRequiredChanged ||
        StateComputeModeHelper<GfxFamily>::isStateComputeModeRequired(csrSizeRequestFlags, this->lastSentThreadArbitrationPolicy != this->requiredThreadArbitrationPolicy)) {
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
    }
}
} // namespace NEO