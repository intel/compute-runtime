/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/command_encoder.inl"
#include "shared/source/command_container/command_encoder_bdw_and_later.inl"
#include "shared/source/command_container/command_encoder_pre_xe2_hpg_core.inl"
#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/gen8/reg_configs.h"

using Family = NEO::Gen8Family;

#include "shared/source/command_container/command_encoder_heap_addressing.inl"
#include "shared/source/command_container/encode_compute_mode_bdw_and_later.inl"
#include "shared/source/command_container/image_surface_state/compression_params_bdw_and_later.inl"

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
    return 0u;
}

template <>
inline size_t EncodeComputeMode<Family>::getSizeForComputeMode() {
    return 0;
}

template <typename Family>
void EncodeComputeMode<Family>::programComputeModeCommand(LinearStream &csr, StateComputeModeProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <>
void EncodeStateBaseAddress<Family>::setSbaAddressesForDebugger(NEO::Debugger::SbaAddresses &sbaAddress, const STATE_BASE_ADDRESS &sbaCmd) {
    sbaAddress.indirectObjectBaseAddress = sbaCmd.getIndirectObjectBaseAddress();
    sbaAddress.dynamicStateBaseAddress = sbaCmd.getDynamicStateBaseAddress();
    sbaAddress.generalStateBaseAddress = sbaCmd.getGeneralStateBaseAddress();
    sbaAddress.instructionBaseAddress = sbaCmd.getInstructionBaseAddress();
    sbaAddress.surfaceStateBaseAddress = sbaCmd.getSurfaceStateBaseAddress();
}

template <>
void EncodeBatchBufferStartOrEnd<Family>::appendBatchBufferStart(MI_BATCH_BUFFER_START &cmd, bool indirect, bool predicate) {
}

} // namespace NEO

#include "shared/source/command_container/command_encoder_enablers.inl"

namespace NEO {
template struct EncodeL3State<Family>;

template void InOrderPatchCommandHelpers::PatchCmd<Family>::patchComputeWalker(uint64_t appendCounterValue);
} // namespace NEO

#include "shared/source/command_container/implicit_scaling_before_xe_hp.inl"