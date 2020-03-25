/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_base.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_plus.inl"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/reg_configs.h"
#include "shared/source/helpers/preamble.h"

namespace NEO {

using Family = TGLLPFamily;

template <>
inline size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device) {
    size_t size = 0;
    if (device.getDefaultEngine().commandStreamReceiver->isRcs()) {
        size += 2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(device.getHardwareInfo());
    }
    return size;
}

template <>
size_t EncodeStates<Family>::getAdjustStateComputeModeSize() {
    return sizeof(typename Family::STATE_COMPUTE_MODE);
}

template <>
void EncodeComputeMode<Family>::adjustComputeMode(LinearStream &csr, uint32_t numGrfRequired, void *const stateComputeModePtr, bool isMultiOsContextCapable) {
    STATE_COMPUTE_MODE *stateComputeMode = static_cast<STATE_COMPUTE_MODE *>(stateComputeModePtr);
    auto buffer = csr.getSpace(sizeof(STATE_COMPUTE_MODE));
    *reinterpret_cast<STATE_COMPUTE_MODE *>(buffer) = *stateComputeMode;
}

template <>
inline void EncodeWA<Family>::encodeAdditionalPipelineSelect(Device &device, LinearStream &stream, bool is3DPipeline) {
    if (device.getDefaultEngine().commandStreamReceiver->isRcs()) {
        PipelineSelectArgs args;
        args.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<Family>::programPipelineSelect(&stream, args, device.getHardwareInfo());
    }
}

template struct EncodeDispatchKernel<Family>;
template struct EncodeStates<Family>;
template struct EncodeMath<Family>;
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
template struct EncodeWA<Family>;
} // namespace NEO
