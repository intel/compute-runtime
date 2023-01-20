/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/logical_state_helper.h"

namespace NEO {

template <typename Family>
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const HardwareInfo &hwInfo, bool hasSharedHandles, bool isRcs) {
    size_t size = 0;
    auto &productHelper = (*ProductHelper::get(hwInfo.platform.eProductFamily));
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier(false);
    }
    size += sizeof(typename Family::STATE_COMPUTE_MODE);
    if (hasSharedHandles) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier(false);
    }
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        size += (2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(hwInfo));
    }
    return size;
}

template <typename Family>
inline void EncodeComputeMode<Family>::programComputeModeCommandWithSynchronization(
    LinearStream &csr, StateComputeModeProperties &properties, const PipelineSelectArgs &args,
    bool hasSharedHandles, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlush, LogicalStateHelper *logicalStateHelper) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, true, hwInfo, isRcs);

    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        PipeControlArgs args;
        NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(csr, args, rootDeviceEnvironment, isRcs);
    }

    EncodeComputeMode<Family>::programComputeModeCommand(csr, properties, rootDeviceEnvironment, logicalStateHelper);

    if (hasSharedHandles) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);
    }

    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, false, hwInfo, isRcs);
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
