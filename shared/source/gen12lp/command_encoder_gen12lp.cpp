/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/command_encoder_from_gen12lp_to_xe2_hpg.inl"
#include "shared/source/command_container/command_encoder_gen12lp_and_xe_hpg.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/command_container/command_encoder_tgllp_and_later.inl"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/reg_configs.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/release_helper/release_helper.h"

#include "encode_surface_state_args.h"

using Family = NEO::Gen12LpFamily;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver.h"

namespace NEO {

template <>
size_t EncodeWA<Family>::getAdditionalPipelineSelectSize(Device &device, bool isRcs) {
    size_t size = 0;
    const auto &productHelper = device.getProductHelper();
    if (isRcs && productHelper.is3DPipelineSelectWARequired()) {
        size += 2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(device.getRootDeviceEnvironment());
    }
    return size;
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
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
                                                      bool is3DPipeline, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        PipelineSelectArgs pipelineSelectArgs = args;
        pipelineSelectArgs.is3DPipelineRequired = is3DPipeline;
        PreambleHelper<Family>::programPipelineSelect(&stream, pipelineSelectArgs, rootDeviceEnvironment);
    }
}

template <>
void EncodeSurfaceState<Family>::encodeExtraBufferParams(EncodeSurfaceStateArgs &args) {
    auto surfaceState = reinterpret_cast<R_SURFACE_STATE *>(args.outMemory);
    const bool isL3Allowed = surfaceState->getMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER);
    if (isL3Allowed) {
        const bool isConstantSurface = args.allocation && args.allocation->getAllocationType() == AllocationType::constantSurface;
        bool useL1 = args.isReadOnly || isConstantSurface;

        if (debugManager.flags.ForceL1Caching.get() != 1) {
            useL1 = false;
        }

        if (useL1) {
            surfaceState->setMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
        }
    }
}

template <>
void EncodeL3State<Family>::encode(CommandContainer &container, bool enableSLM) {
}

template <>
void EncodeStoreMMIO<Family>::appendFlags(MI_STORE_REGISTER_MEM *storeRegMem, bool workloadPartition) {
    storeRegMem->setMmioRemapEnable(true);
}

template <>
void EncodeSurfaceState<Family>::appendImageCompressionParams(R_SURFACE_STATE *surfaceState, GraphicsAllocation *allocation,
                                                              GmmHelper *gmmHelper, bool imageFromBuffer, GMM_YUV_PLANE_ENUM plane) {
}

template <>
inline void EncodeSurfaceState<Family>::encodeExtraCacheSettings(R_SURFACE_STATE *surfaceState, const EncodeSurfaceStateArgs &args) {}

template <>
inline void EncodeWA<Family>::setAdditionalPipeControlFlagsForNonPipelineStateCommand(PipeControlArgs &args) {}

template <>
bool EncodeEnableRayTracing<Family>::is48bResourceNeededForRayTracing() {
    return true;
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template struct EncodeL3State<Family>;

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
} // namespace NEO

#include "shared/source/command_container/implicit_scaling_before_xe_hp.inl"