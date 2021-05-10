/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_extended/cmdlist_extended.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_base.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"
#include "level_zero/core/source/gen12lp/definitions/cache_flush_gen12lp.inl"

namespace L0 {
template struct CommandListCoreFamily<IGFX_GEN12LP_CORE>;

static CommandListPopulateFactory<IGFX_ALDERLAKE_S, CommandListProductFamily<IGFX_ALDERLAKE_S>>
    populateADLS;

static CommandListImmediatePopulateFactory<IGFX_ALDERLAKE_S, CommandListImmediateProductFamily<IGFX_ALDERLAKE_S>>
    populateADLSImmediate;

} // namespace L0