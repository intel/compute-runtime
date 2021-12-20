/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_base.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"

#include "cache_flush_gen12lp.inl"
#include "cmdlist_extended.inl"

namespace L0 {
template <>
void CommandListCoreFamily<IGFX_GEN12LP_CORE>::clearComputeModePropertiesIfNeeded(bool requiresCoherency, uint32_t numGrfRequired, uint32_t threadArbitrationPolicy) {
    finalStreamState.stateComputeMode = {};
    finalStreamState.stateComputeMode.setProperties(false, numGrfRequired, threadArbitrationPolicy);
}

template struct CommandListCoreFamily<IGFX_GEN12LP_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_GEN12LP_CORE>;

} // namespace L0