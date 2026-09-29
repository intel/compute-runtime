/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3p.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe2_hpg_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"
#include "implicit_args.h"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE2_HPG_CORE;

template struct CommandListCoreFamily<gfxCoreFamily>;
template struct CommandListCoreFamilyImmediate<gfxCoreFamily>;

static CommandListPopulateFactory<gfxCoreFamily, CommandListCoreFamily<gfxCoreFamily>> populateXe2HpgCore;
static CommandListImmediatePopulateFactory<gfxCoreFamily, CommandListCoreFamilyImmediate<gfxCoreFamily>> populateXe2HpgCoreImmediate;

} // namespace L0
