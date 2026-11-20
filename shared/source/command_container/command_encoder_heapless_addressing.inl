/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"

#include "implicit_args.h"

namespace NEO {

template <typename Family>
void EncodeDispatchKernel<Family>::programInlineDataHeapless(uint8_t *inlineDataPtr, EncodeDispatchKernelArgs &args, CommandContainer &container, uint64_t offsetThreadData, uint64_t scratchPtr) {

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
            if (isDefined(scratchPointerAddress.pointerSize) && isValidOffset(scratchPointerAddress.offset) &&
                (static_cast<uint32_t>(scratchPointerAddress.offset) + scratchPointerAddress.pointerSize <= inlineDataSize)) {
                uint32_t maxBytesToCopy = std::max(0, static_cast<int32_t>(inlineDataSize - scratchPointerAddress.offset));
                memcpy_s(inlineDataPtr + scratchPointerAddress.offset, maxBytesToCopy, &scratchPtr, scratchPointerAddress.pointerSize);
            }
        }
    }
}

template <typename Family>
bool EncodeDispatchKernel<Family>::getScratchAddressForImmediatePatching(CommandContainer &container, EncodeDispatchKernelArgs &args, uint64_t &scratchAddress, uint32_t &scratchSlot0SizeAllocated) {

    scratchAddress = 0u;
    scratchSlot0SizeAllocated = 0u;

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

        if (!EncodeDispatchKernel<Family>::setScratchAddress(scratchAddress, requiredScratchSlot0Size, requiredScratchSlot1Size, ssh, *csr)) {
            return false;
        }

        scratchSlot0SizeAllocated = csr->getPerThreadScratchSizeSlot0Allocated();
    }

    return true;
}

template <typename Family>
void EncodeDispatchKernel<Family>::patchScratchAddressInImplicitArgs(ImplicitArgs &implicitArgs, uint64_t scratchAddress, bool scratchPtrPatchingRequired) {

    if (scratchPtrPatchingRequired) {
        implicitArgs.setScratchBufferPtr(scratchAddress);
    }
}

template <typename Family>
void EncodeStates<Family>::adjustSamplerStateBorderColor(void *samplerState, const void *borderColorState) {
    using SAMPLER_STATE = typename Family::SAMPLER_STATE;
    constexpr auto borderColorOffsetInSamplerState = 16u;

    constexpr auto borderColorSize = sizeof(SAMPLER_STATE) - borderColorOffsetInSamplerState;
    void *borderColorStateInSamplerState = ptrOffset(samplerState, borderColorOffsetInSamplerState);
    memcpy_s(borderColorStateInSamplerState, borderColorSize, borderColorState, borderColorSize);
}

} // namespace NEO
