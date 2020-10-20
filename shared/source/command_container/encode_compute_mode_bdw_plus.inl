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
}

template <typename Family>
void EncodeStoreMMIO<Family>::remapOffset(MI_STORE_REGISTER_MEM *pStoreRegMem) {
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_MEM *pMiLoadReg) {
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_REG *pMiLoadReg) {
}

template <typename Family>
bool EncodeSetMMIO<Family>::isRemapApplicable(uint32_t offset) {
    return false;
}

} // namespace NEO