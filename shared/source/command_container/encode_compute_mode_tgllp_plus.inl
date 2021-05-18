/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {
template <typename Family>
void EncodeStates<Family>::adjustStateComputeMode(LinearStream &csr, uint32_t numGrfRequired, void *const stateComputeModePtr,
                                                  bool isMultiOsContextCapable, bool requiresCoherency, bool useGlobalAtomics, bool areMultipleSubDevicesInContext) {
    StreamProperties properties{};
    properties.setStateComputeModeProperties(requiresCoherency, numGrfRequired, isMultiOsContextCapable, useGlobalAtomics, areMultipleSubDevicesInContext);
    EncodeComputeMode<Family>::adjustComputeMode(csr, stateComputeModePtr, properties.stateComputeMode);
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
