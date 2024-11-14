/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"

namespace NEO {

template <typename Family>
size_t EncodeDispatchKernel<Family>::getScratchPtrOffsetOfImplicitArgs() {
    return 0;
}

template <typename Family>
void EncodeSurfaceState<Family>::setPitchForScratch(R_SURFACE_STATE *surfaceState, uint32_t pitch, const ProductHelper &productHelper) {
    surfaceState->setSurfacePitch(pitch);
}

template <typename Family>
uint32_t EncodeSurfaceState<Family>::getPitchForScratchInBytes(R_SURFACE_STATE *surfaceState, const ProductHelper &productHelper) {
    return surfaceState->getSurfacePitch();
}

template <typename Family>
void EncodeSemaphore<Family>::appendSemaphoreCommand(MI_SEMAPHORE_WAIT &cmd, uint64_t compareData, bool indirect, bool useQwordData, bool switchOnUnsuccessful) {
    constexpr uint64_t upper32b = static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) << 32;
    UNRECOVERABLE_IF(useQwordData || (compareData & upper32b));
}

template <typename Family>
template <bool isHeapless>
void EncodeDispatchKernel<Family>::setScratchAddress(uint64_t &scratchAddress, uint32_t requiredScratchSlot0Size, uint32_t requiredScratchSlot1Size, IndirectHeap *ssh, CommandStreamReceiver &submissionCsr) {
}

template <typename Family>
template <typename InterfaceDescriptorType>
void EncodeDispatchKernel<Family>::encodeEuSchedulingPolicy(InterfaceDescriptorType *pInterfaceDescriptor, const KernelDescriptor &kernelDesc, int32_t defaultPipelinedThreadArbitrationPolicy) {
}

template <typename Family>
bool EncodeDispatchKernel<Family>::singleTileExecImplicitScalingRequired(bool cooperativeKernel) {
    return cooperativeKernel;
}

} // namespace NEO
