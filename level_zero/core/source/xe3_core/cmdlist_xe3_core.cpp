/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe2_hpg_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template struct CommandListCoreFamily<IGFX_XE3_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE3_CORE>;

} // namespace L0
