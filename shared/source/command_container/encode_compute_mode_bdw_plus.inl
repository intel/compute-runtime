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
} // namespace NEO