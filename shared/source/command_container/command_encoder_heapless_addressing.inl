/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"

#include "implicit_args.h"

namespace NEO {

template <typename Family>
template <bool heaplessModeEnabled>
void EncodeDispatchKernel<Family>::programInlineDataHeapless(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData, uint64_t scratchPtr) {

    if constexpr (heaplessModeEnabled) {
        if (!args.makeCommandView) {
            const auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
            auto indirectDataPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress;
            auto heap = container.getIndirectHeap(HeapType::indirectObject);
            auto indirectDataAddress = heap->getHeapGpuBase() + offsetThreadData;
            uint32_t inlineDataSize = DefaultWalkerType::getInlineDataSize();
            if (isDefined(indirectDataPointerAddress.pointerSize) && isValidOffset(indirectDataPointerAddress.offset)) {
                uint32_t maxBytesToCopy = std::max(0, static_cast<int32_t>(inlineDataSize - indirectDataPointerAddress.offset));
                memcpy_s(inlineDataPtr + indirectDataPointerAddress.offset, maxBytesToCopy, &indirectDataAddress, indirectDataPointerAddress.pointerSize);
            }

            if (args.immediateScratchAddressPatching) {
                auto scratchPointerAddress = kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress;
                if (isDefined(scratchPointerAddress.pointerSize) && isValidOffset(scratchPointerAddress.offset)) {
                    uint32_t maxBytesToCopy = std::max(0, static_cast<int32_t>(inlineDataSize - scratchPointerAddress.offset));
                    memcpy_s(inlineDataPtr + scratchPointerAddress.offset, maxBytesToCopy, &scratchPtr, scratchPointerAddress.pointerSize);
                }
            }
        }
    }
}

template <typename Family>
template <bool isHeapless>
uint64_t EncodeDispatchKernel<Family>::getScratchAddressForImmediatePatching(CommandContainer &container, EncodeDispatchKernelArgs &args) {

    uint64_t scratchAddress = 0u;
    if constexpr (isHeapless) {
        if (args.immediateScratchAddressPatching) {
            const auto &kernelDescriptor = args.dispatchInterface->getKernelDescriptor();
            auto requiredScratchSlot0Size = kernelDescriptor.kernelAttributes.perThreadScratchSize[0];
            auto requiredScratchSlot1Size = kernelDescriptor.kernelAttributes.perThreadScratchSize[1];
            auto csr = container.getImmediateCmdListCsr();
            NEO::IndirectHeap *ssh = nullptr;
            if (csr->getGlobalStatelessHeapAllocation() != nullptr) {
                ssh = csr->getGlobalStatelessHeap();
            } else {
                ssh = args.surfaceStateHeap ? args.surfaceStateHeap : container.getIndirectHeap(HeapType::surfaceState);
            }

            EncodeDispatchKernel<Family>::template setScratchAddress<isHeapless>(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, *csr);
        }
    }
    return scratchAddress;
}

template <typename Family>
template <bool isHeapless>
void EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrPatchingRequired) {
    if constexpr (isHeapless) {
        if (scratchPtrPatchingRequired) {
            implicitArgs.setScratchBufferPtr(scratchAddress);
        }
    }
}

template <typename Family>
void EncodeStates<Family>::adjustSamplerStateBorderColor(SAMPLER_STATE &samplerState, const SAMPLER_BORDER_COLOR_STATE &borderColorState) {
    constexpr auto borderColorOffsetInSamplerState = 16u;

    void *borderColorStateInSamplerState = reinterpret_cast<void *>(reinterpret_cast<uint8_t *>(&samplerState) + borderColorOffsetInSamplerState);
    memcpy_s(borderColorStateInSamplerState, sizeof(SAMPLER_BORDER_COLOR_STATE), &borderColorState, sizeof(SAMPLER_BORDER_COLOR_STATE));
}

} // namespace NEO
