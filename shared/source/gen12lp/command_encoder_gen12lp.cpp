/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/reg_configs.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"

using Family = NEO::TGLLPFamily;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_tgllp_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_tgllp_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

template <>
size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device) {
    size_t size = 0;
    if (device.getDefaultEngine().commandStreamReceiver->isRcs()) {
        size += 2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(device.getHardwareInfo());
    }
    return size;
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const HardwareInfo &hwInfo) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;

    STATE_COMPUTE_MODE stateComputeMode = Family::cmdInitStateComputeMode;
    auto maskBits = stateComputeMode.getMaskBits();

    FORCE_NON_COHERENT coherencyValue = (properties.isCoherencyRequired.value == 1) ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED
                                                                                    : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    stateComputeMode.setForceNonCoherent(coherencyValue);
    maskBits |= Family::stateComputeModeForceNonCoherentMask;

    stateComputeMode.setMaskBits(maskBits);

    auto buffer = csr.getSpace(sizeof(STATE_COMPUTE_MODE));
    *reinterpret_cast<STATE_COMPUTE_MODE *>(buffer) = stateComputeMode;
}

template <>
void EncodeWA<Family>::encodeAdditionalPipelineSelect(LinearStream &stream, const PipelineSelectArgs &args,
                                                      bool is3DPipeline, const HardwareInfo &hwInfo, bool isRcs) {
    const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired() && isRcs) {
        PipelineSelectArgs pipelineSelectArgs = args;
        pipelineSelectArgs.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<Family>::programPipelineSelect(&stream, pipelineSelectArgs, hwInfo);
    }
}

template <>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    const bool isL3Allowed = surfaceState->getMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    if (isL3Allowed) {
        const bool isConstantSurface = args.allocation && args.allocation->getAllocationType() == AllocationType::CONSTANT_SURFACE;
        bool useL1 = args.isReadOnly || isConstantSurface;

        if (DebugManager.flags.ForceL1Caching.get() != 1) {
            useL1 = false;
        }

        if (useL1) {
            surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
        }
    }
}

template <>
bool EncodeSurfaceState<Family>::doBindingTablePrefetch() {
    return false;
}

template <>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
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
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
} // namespace NEO
