/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template <>
void CommandListCoreFamily<IGFX_XE_HP_CORE>::clearComputeModePropertiesIfNeeded(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    finalStreamState.stateComputeMode = {};
    finalStreamState.stateComputeMode.setProperties(requiresCoherency, numGrfRequired, threadArbitrationPolicy);
}

template struct CommandListCoreFamily<IGFX_XE_HP_CORE>;

template <>
struct CommandListProductFamily<IGFX_XE_HP_SDV> : public CommandListCoreFamily<IGFX_XE_HP_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;
};

static CommandListPopulateFactory<IGFX_XE_HP_SDV, CommandListProductFamily<IGFX_XE_HP_SDV>>
    populateXEHP;

template <>
struct CommandListImmediateProductFamily<IGFX_XE_HP_SDV> : public CommandListCoreFamilyImmediate<IGFX_XE_HP_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};

static CommandListImmediatePopulateFactory<IGFX_XE_HP_SDV, CommandListImmediateProductFamily<IGFX_XE_HP_SDV>>
    populateXEHPImmediate;

} // namespace L0
