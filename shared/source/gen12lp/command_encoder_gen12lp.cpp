/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_base.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "opencl/source/gen12lp/reg_configs.h"

namespace NEO {

using Family = TGLLPFamily;

template <>
void EncodeStates<Family>::adjustStateComputeMode(CommandContainer &container) {
    auto stateComputeModeCmd = Family::cmdInitStateComputeMode;
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename Family::STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    stateComputeModeCmd.setForceNonCoherent(FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);

    stateComputeModeCmd.setMaskBits(Family::stateComputeModeForceNonCoherentMask);

    // Commit our commands to the commandStream
    auto buffer = container.getCommandStream()->getSpace(sizeof(stateComputeModeCmd));
    *(decltype(stateComputeModeCmd) *)buffer = stateComputeModeCmd;
}
template <>
size_t EncodeStates<Family>::getAdjustStateComputeModeSize() {
    return sizeof(typename Family::STATE_COMPUTE_MODE);
}

template struct EncodeDispatchKernel<Family>;
template struct EncodeStates<Family>;
template struct EncodeMathMMIO<Family>;
template struct EncodeIndirectParams<Family>;
template struct EncodeSetMMIO<Family>;
template struct EncodeL3State<Family>;
template struct EncodeMediaInterfaceDescriptorLoad<Family>;
template struct EncodeStateBaseAddress<Family>;
template struct EncodeStoreMMIO<Family>;
template struct EncodeSurfaceState<Family>;
template struct EncodeAtomic<Family>;
template struct EncodeSempahore<Family>;
template struct EncodeBatchBufferStartOrEnd<Family>;
template struct EncodeMiFlushDW<Family>;
} // namespace NEO
