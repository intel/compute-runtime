/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {
template <>
void CommandListCoreFamily<IGFX_XE_HPG_CORE>::clearComputeModePropertiesIfNeeded(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    finalStreamState.stateComputeMode = {};
    finalStreamState.stateComputeMode.setProperties(requiresCoherency, numGrfRequired, threadArbitrationPolicy);
}

template struct CommandListCoreFamily<IGFX_XE_HPG_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPG_CORE>;

} // namespace L0