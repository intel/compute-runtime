/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

namespace NEO {
template <typename Family>
template <bool heaplessModeEnabled>
void EncodeDispatchKernel<Family>::programInlineDataHeapless(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData, uint64_t scratchPtr) {
}

template <typename Family>
template <bool heaplessModeEnabled>
uint64_t EncodeDispatchKernel<Family>::getScratchAddressForImmediatePatching(CommandContainer &container, EncodeDispatchKernelArgs &args) {
    return 0u;
}

template <typename Family>
template <bool heaplessModeEnabled>
void EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrRequired) {
}

template <typename Family>
void EncodeStates<Family>::adjustSamplerStateBorderColor(SAMPLER_STATE &samplerState, const SAMPLER_BORDER_COLOR_STATE &borderColorState) {
}

} // namespace NEO
