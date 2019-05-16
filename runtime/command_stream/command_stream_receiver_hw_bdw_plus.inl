/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw_base.inl"

namespace NEO {

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getSshHeapSize() {
    return defaultHeapSize;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config) {
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    if (csrSizeRequestFlags.l3ConfigChanged && this->isPreambleSent) {
        // Add a PIPE_CONTROL w/ CS_stall
        auto pCmd = (PIPE_CONTROL *)csr.getSpace(sizeof(PIPE_CONTROL));
        *pCmd = GfxFamily::cmdInitPipeControl;
        pCmd->setCommandStreamerStallEnable(true);
        pCmd->setDcFlushEnable(true);
        addClearSLMWorkAround(pCmd);

        PreambleHelper<GfxFamily>::programL3(&csr, newL3Config);
        this->lastSentL3Config = newL3Config;
    }
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredStateBaseAddressSize() const {
    return sizeof(typename GfxFamily::STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForL3Config() const {
    if (!this->isPreambleSent) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    } else if (csrSizeRequestFlags.l3ConfigChanged) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) + sizeof(typename GfxFamily::PIPE_CONTROL);
    }
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPipelineSelect(LinearStream &commandStream, DispatchFlags &dispatchFlags) {
    if (csrSizeRequestFlags.mediaSamplerConfigChanged || !isPreambleSent) {
        PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, dispatchFlags);
        this->lastMediaSamplerConfig = dispatchFlags.mediaSamplerRequired;
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPipelineSelect() const {
    if (csrSizeRequestFlags.mediaSamplerConfigChanged || !isPreambleSent) {
        return sizeof(typename GfxFamily::PIPELINE_SELECT);
    }
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::createScratchSpaceController() {
    scratchSpaceController = std::make_unique<ScratchSpaceControllerBase>(executionEnvironment, *internalAllocationStorage.get());
}

} // namespace NEO
