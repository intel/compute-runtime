/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {
template <typename Family>
void EncodeStates<Family>::adjustStateComputeMode(LinearStream &csr, uint32_t numGrfRequired, void *const stateComputeModePtr, bool isMultiOsContextCapable, bool requiresCoherency) {
    using STATE_COMPUTE_MODE = typename Family::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    STATE_COMPUTE_MODE stateComputeMode = (stateComputeModePtr != nullptr) ? *(static_cast<STATE_COMPUTE_MODE *>(stateComputeModePtr)) : Family::cmdInitStateComputeMode;
    FORCE_NON_COHERENT coherencyValue = !requiresCoherency ? FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT : FORCE_NON_COHERENT::FORCE_NON_COHERENT_FORCE_DISABLED;
    stateComputeMode.setForceNonCoherent(coherencyValue);

    stateComputeMode.setMaskBits(stateComputeMode.getMaskBits() | Family::stateComputeModeForceNonCoherentMask);

    EncodeComputeMode<Family>::adjustComputeMode(csr, numGrfRequired, &stateComputeMode, isMultiOsContextCapable);
}

template <typename Family>
void EncodeStoreMMIO<Family>::remapOffset(MI_STORE_REGISTER_MEM *pStoreRegMem) {
    pStoreRegMem->setMmioRemapEnable(true);
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
bool EncodeSetMMIO<Family>::isRemapApplicable(uint32_t offset) {
    return (0x2000 <= offset && offset <= 0x27ff) ||
           (0x4200 <= offset && offset <= 0x420f) ||
           (0x4400 <= offset && offset <= 0x441f);
}

} // namespace NEO