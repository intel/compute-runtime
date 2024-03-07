/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {

template <typename Family>
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const RootDeviceEnvironment &rootDeviceEnvironment, bool hasSharedHandles, bool isRcs) {
    size_t size = 0;
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier(false);
    }
    size += sizeof(typename Family::STATE_COMPUTE_MODE);
    if (hasSharedHandles) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier(false);
    }
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        size += (2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(rootDeviceEnvironment));
    }
    return size;
}

template <typename GfxFamily>
inline size_t EncodeComputeMode<GfxFamily>::getSizeForComputeMode() {
    return sizeof(typename GfxFamily::STATE_COMPUTE_MODE);
}

template <typename Family>
inline void EncodeComputeMode<Family>::programComputeModeCommandWithSynchronization(
    LinearStream &csr, StateComputeModeProperties &properties, const PipelineSelectArgs &args,
    bool hasSharedHandles, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlush) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, true, rootDeviceEnvironment, isRcs);
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        PipeControlArgs args;
        NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(csr, args, rootDeviceEnvironment, isRcs);
    }

    EncodeComputeMode<Family>::programComputeModeCommand(csr, properties, rootDeviceEnvironment);

    if (hasSharedHandles) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);
    }

    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, false, rootDeviceEnvironment, isRcs);
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_MEM *pMiLoadReg) {
    if (isRemapApplicable(pMiLoadReg->getRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnable(true);
    }
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_REG *pMiLoadReg) {
    if (isRemapApplicable(pMiLoadReg->getSourceRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnableSource(true);
    }
    if (isRemapApplicable(pMiLoadReg->getDestinationRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnableDestination(true);
    }
}

template <typename Family>
inline bool EncodeSetMMIO<Family>::isRemapApplicable(uint32_t offset) {
    return (0x2000 <= offset && offset <= 0x27ff) ||
           (0x4200 <= offset && offset <= 0x420f) ||
           (0x4400 <= offset && offset <= 0x441f);
}

template <typename Family>
void EncodeSurfaceState<Family>::disableCompressionFlags(R_SURFACE_STATE *surfaceState) {
    surfaceState->setAuxiliarySurfaceMode(Family::RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    surfaceState->setMemoryCompressionEnable(false);
}
} // namespace NEO
