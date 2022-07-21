/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen11/hw_cmds_base.h"
#include "shared/source/gen11/reg_configs.h"

#include "reg_configs_common.h"

using Family = NEO::ICLFamily;

#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/encode_compute_mode_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"

namespace NEO {

template <>
bool EncodeSurfaceState<Family>::doBindingTablePrefetch() {
    return false;
}

template <>
void EncodeSurfaceState<Family>::setAuxParamsForMCSCCS(R_SURFACE_STATE *surfaceState) {
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
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const HardwareInfo &hwInfo, bool hasSharedHandles, bool isRcs) {
    return sizeof(typename Family::PIPE_CONTROL) + 2u * sizeof(typename Family::MI_LOAD_REGISTER_IMM);
}

template <>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties,
                                                          const HardwareInfo &hwInfo, LogicalStateHelper *logicalStateHelper) {
    using PIPE_CONTROL = typename Family::PIPE_CONTROL;

    if (properties.threadArbitrationPolicy.isDirty) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);

        LriHelper<Family>::program(&csr,
                                   RowChickenReg4::address,
                                   RowChickenReg4::regDataForArbitrationPolicy[properties.threadArbitrationPolicy.value],
                                   false);
    }
    if (properties.isCoherencyRequired.isDirty) {
        auto nonCoherentEnable = !properties.isCoherencyRequired.value;
        LriHelper<Family>::program(&csr,
                                   gen11HdcModeRegister::address,
                                   DwordBuilder::build(gen11HdcModeRegister::forceNonCoherentEnableBit, true, nonCoherentEnable),
                                   false);
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
template struct EncodeMemoryPrefetch<Family>;
template struct EncodeWA<Family>;
template struct EncodeMiArbCheck<Family>;
template struct EncodeComputeMode<Family>;
template struct EncodeEnableRayTracing<Family>;
template struct EncodeNoop<Family>;
template struct EncodeStoreMemory<Family>;
template struct EncodeMemoryFence<Family>;
} // namespace NEO
