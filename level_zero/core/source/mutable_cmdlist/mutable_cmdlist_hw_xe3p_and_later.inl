/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_indirect_data.h"

namespace L0::MCL {

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateScratchAddress(size_t scratchAddressPatchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker, MutableIndirectData *newKernelIndirectData) {
    if (isUndefined(scratchAddressPatchIndex)) {
        return;
    }

    auto scratchPatchAddress = this->getCurrentScratchPatchAddress(scratchAddressPatchIndex);
    auto newScratchOffset = newWalker.getScratchOffset();
    if (isDefined(newScratchOffset) && (newScratchOffset >= newWalker.getInlineDataSize())) {
        newKernelIndirectData->setAddress(newScratchOffset, scratchPatchAddress, sizeof(uint64_t));
    } else {
        newWalker.updateWalkerScratchPatchAddress(scratchPatchAddress);
    }
    this->updateCmdListScratchPatchCommand(scratchAddressPatchIndex, oldWalker, newWalker, newKernelIndirectData);
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint64_t MutableCommandListCoreFamily<gfxCoreFamily>::getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    UNRECOVERABLE_IF(scratchAddressPatchIndex >= commandsToPatch.size());
    auto &scratchPatch = std::get<PatchComputeWalkerInlineDataScratch>(commandsToPatch[scratchAddressPatchIndex]);

    auto currentScratchPatchAddress = scratchPatch.scratchAddressAfterPatch;
    if (currentScratchPatchAddress == 0) {
        return 0;
    }

    return currentScratchPatchAddress + scratchPatch.baseAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker, MutableIndirectData *newKernelIndirectData) {
    auto newScratchOffset = newWalker.getScratchOffset();
    auto oldScratchOffset = oldWalker.getScratchOffset();
    if (newScratchOffset != oldScratchOffset) {
        // scratch offset has changed: update scratch patch command with current scratch offset
        const auto inlineDataSize = newWalker.getInlineDataSize();
        const bool scratchInCrossThreadData = isDefined(newScratchOffset) && (newScratchOffset >= inlineDataSize);

        size_t patchSize = isDefined(newScratchOffset) ? sizeof(uint64_t) : undefined<size_t>;
        size_t patchOffset = undefined<size_t>;
        void *patchDestination = newWalker.getWalkerCmdPointer();
        if (scratchInCrossThreadData) {
            patchDestination = newKernelIndirectData->getCrossThreadDataBaseAddress();
            patchOffset = newScratchOffset - inlineDataSize;
        } else if (isDefined(newScratchOffset)) {
            patchOffset = newWalker.getInlineDataOffset() + newScratchOffset;
        }

        uint64_t patchGpuAddress = 0u;
        if (isDefined(newScratchOffset)) {
            auto patchAllocation = this->commandContainer.findGraphicsAllocationForCpuAddress(patchDestination);
            UNRECOVERABLE_IF(patchAllocation == nullptr);
            patchGpuAddress = patchAllocation->getGpuAddress() +
                              (reinterpret_cast<uintptr_t>(patchDestination) - reinterpret_cast<uintptr_t>(patchAllocation->getUnderlyingBuffer()));
        }

        auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
        UNRECOVERABLE_IF(patchIndex >= commandsToPatch.size());

        auto &patch = std::get<PatchComputeWalkerInlineDataScratch>(commandsToPatch[patchIndex]);
        patch.pDestination = patchDestination;
        patch.gpuAddress = patchGpuAddress;
        patch.patchSize = patchSize;
        patch.offset = patchOffset;

        if (isUndefined(oldScratchOffset) && isDefined(newScratchOffset)) {
            this->activeScratchPatchElements += 1u;
        } else if (isDefined(oldScratchOffset) && isUndefined(newScratchOffset)) {
            this->activeScratchPatchElements -= 1u;
        }
    }
}

} // namespace L0::MCL
