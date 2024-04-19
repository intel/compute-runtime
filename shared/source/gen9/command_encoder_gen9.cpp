/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds_base.h"
#include "shared/source/gen9/reg_configs.h"

using Family = NEO::Gen9Family;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_container/encode_compute_mode_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"

#include "stream_properties.inl"

namespace NEO {
template <>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState, const ReleaseHelper *releaseHelper) {
}

template <>
void EncodeSurfaceState<Family>::setClearColorParams(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
}

template <>
void EncodeSurfaceState<Family>::setFlagsForMediaCompression(R_SURFACE_STATE *surfaceState, Gmm *gmm) {
    if (gmm->gmmResourceInfo->getResourceFlags()->Info.MediaCompressed) {
        surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}
template <typename Family>
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const RootDeviceEnvironment &rootDeviceEnvironment, bool hasSharedHandles, bool isRcs) {
    return sizeof(typename Family::PIPE_CONTROL) + sizeof(typename Family::MI_LOAD_REGISTER_IMM);
}

template <>
inline size_t EncodeComputeMode<Family>::getSizeForComputeMode() {
    return 0;
}

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties,
                                                          const RootDeviceEnvironment &rootDeviceEnvironment) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;
    UNRECOVERABLE_IF(properties.threadArbitrationPolicy.value == ThreadArbitrationPolicy::NotPresent);

    if (properties.threadArbitrationPolicy.isDirty) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);

        LriHelper<Gen9Family>::program(&csr,
                                       DebugControlReg2::address,
                                       DebugControlReg2::getRegData(properties.threadArbitrationPolicy.value),
                                       false,
                                       false);
    }
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template struct EncodeL3State<Family>;

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
} // namespace NEO

#include "shared/source/command_container/implicit_scaling_before_xe_hp.inl"
