/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe2_hpg_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe3p_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"
#include "implicit_args.h"

namespace L0 {

template <>
size_t CommandListCoreFamily<NEO::xe3pCoreEnumValue>::getReserveSshSize() {
    constexpr size_t maxPtssSteps = 16;
    constexpr size_t maxExtednedPtssSteps = 19;
    constexpr size_t numSlotsPerStep = 2;
    constexpr size_t numSteps = 2;
    constexpr size_t startSlotIndex = 1;

    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    auto &productHelper = device->getProductHelper();
    if (productHelper.isAvailableExtendedScratch()) {
        return (maxExtednedPtssSteps * numSlotsPerStep + startSlotIndex) * numSteps * sizeof(RENDER_SURFACE_STATE);
    }

    return (maxPtssSteps * numSlotsPerStep + startSlotIndex) * numSteps * sizeof(RENDER_SURFACE_STATE);
}

template <>
void CommandListCoreFamily<NEO::xe3pCoreEnumValue>::setAdditionalBlitProperties(NEO::BlitProperties &blitProperties, Event *signalEvent, uint64_t forceAggregatedEventIncValue, bool useAdditionalTimestamp) {}

template <>
bool CommandListCoreFamily<NEO::xe3pCoreEnumValue>::kernelMemoryPrefetchEnabled() const { return NEO::debugManager.flags.EnableMemoryPrefetch.get() == 1; }

template struct CommandListCoreFamily<NEO::xe3pCoreEnumValue>;
template struct CommandListCoreFamilyImmediate<NEO::xe3pCoreEnumValue>;

} // namespace L0
