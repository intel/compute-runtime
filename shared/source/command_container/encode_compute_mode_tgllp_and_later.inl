/*
 * Copyright (C) 2020-2022 Intel Corporation
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
    auto &hwInfoConfig = (*HwInfoConfig::get(hwInfo.platform.eProductFamily));
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        size += sizeof(typename Family::PIPE_CONTROL);
    }
    size += sizeof(typename Family::STATE_COMPUTE_MODE);
    if (hasSharedHandles) {
        size += sizeof(typename Family::PIPE_CONTROL);
    }
    if (hwInfoConfig.is3DPipelineSelectWARequired() && isRcs) {
        size += (2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(hwInfo));
    }
    return size;
}

template <typename Family>
inline void EncodeComputeMode<Family>::programComputeModeCommandWithSynchronization(
    LinearStream &csr, StateComputeModeProperties &properties, const PipelineSelectArgs &args,
    bool hasSharedHandles, const HardwareInfo &hwInfo, bool isRcs, LogicalStateHelper *logicalStateHelper) {

    using PIPE_CONTROL = typename Family::PIPE_CONTROL;

    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, true, hwInfo, isRcs);

    auto &hwInfoConfig = (*HwInfoConfig::get(hwInfo.platform.eProductFamily));
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        PipeControlArgs args;
        args.dcFlushEnable = MemorySynchronizationCommands<Family>::getDcFlushEnable(true, hwInfo);
        NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(csr, args, hwInfo, isRcs);
    }

    EncodeComputeMode<Family>::programComputeModeCommand(csr, properties, hwInfo, logicalStateHelper);

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
