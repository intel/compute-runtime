/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.h"

namespace L0::MCL {

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateScratchAddress(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) {
}

template <GFXCORE_FAMILY gfxCoreFamily>
uint64_t MutableCommandListCoreFamily<gfxCoreFamily>::getCurrentScratchPatchAddress(size_t scratchAddressPatchIndex) const {
    return 0u;
}

template <GFXCORE_FAMILY gfxCoreFamily>
void MutableCommandListCoreFamily<gfxCoreFamily>::updateCmdListScratchPatchCommand(size_t patchIndex, MutableComputeWalker &oldWalker, MutableComputeWalker &newWalker) {
}

} // namespace L0::MCL
