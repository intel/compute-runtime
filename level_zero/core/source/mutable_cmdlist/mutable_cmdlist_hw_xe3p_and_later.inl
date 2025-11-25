/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"

namespace L0::MCL {

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateScratchAddress(size_t scratchAddressPatchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) {
    if (isUndefined(scratchAddressPatchIndex)) {
        return;
    }

    auto scratchPatchAddress = this->getCurrentScratchPatchAddress(scratchAddressPatchIndex);
    newWalker.updateWalkerScratchPatchAddress(scratchPatchAddress);
    this->updateCmdListScratchPatchCommand(scratchAddressPatchIndex, oldWalker, newWalker);
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint64_t MutableCommandListCoreFamily<gfxCoreFamily>::getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const {
    auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
    UNRECOVERABLE_IF(scratchAddressPatchIndex >= commandsToPatch.size());

    auto currentScratchPatchAddress = commandsToPatch[scratchAddressPatchIndex].scratchAddressAfterPatch;
    if (currentScratchPatchAddress == 0) {
        return 0;
    }

    return currentScratchPatchAddress + commandsToPatch[scratchAddressPatchIndex].baseAddress;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) {
    auto newScratchOffset = newWalker.getScratchOffset();
    auto oldScratchOffset = oldWalker.getScratchOffset();
    if (newScratchOffset != oldScratchOffset) {
        // scratch offset has changed: update scratch patch command with current scratch offset
        size_t patchSize = isDefined(newScratchOffset) ? sizeof(uint64_t) : undefined<size_t>;
        size_t patchOffset = isDefined(newScratchOffset) ? newWalker.getInlineDataOffset() + newScratchOffset : undefined<size_t>;

        auto &commandsToPatch = CommandListCoreFamily<gfxCoreFamily>::commandsToPatch;
        UNRECOVERABLE_IF(patchIndex >= commandsToPatch.size());

        auto &patch = commandsToPatch[patchIndex];
        patch.pDestination = newWalker.getWalkerCmdPointer();
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
