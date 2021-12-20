/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"
#include "level_zero/core/source/xe_hpc_core/cmdlist_xe_hpc_core.inl"

#include "cmdlist_extended.inl"

namespace L0 {
template <>
void CommandListCoreFamily<IGFX_XE_HPC_CORE>::applyMemoryRangesBarrier(uint32_t numRanges,
                                                                       const size_t *pRangeSizes,
                                                                       const void **pRanges) {

    increaseCommandStreamSpace(NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSinglePipeControl());

    NEO::PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    NEO::MemorySynchronizationCommands<GfxFamily>::addPipeControl(*commandContainer.getCommandStream(), args);
}

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;

template <>
struct CommandListProductFamily<IGFX_PVC> : public CommandListCoreFamily<IGFX_XE_HPC_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;
};

static CommandListPopulateFactory<IGFX_PVC, CommandListProductFamily<IGFX_PVC>>
    populatePVC;

template <>
struct CommandListImmediateProductFamily<IGFX_PVC> : public CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};

static CommandListImmediatePopulateFactory<IGFX_PVC, CommandListImmediateProductFamily<IGFX_PVC>>
    populatePVCImmediate;
} // namespace L0
