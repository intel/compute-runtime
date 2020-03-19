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
} // namespace NEO