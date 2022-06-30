/*
 * Copyright (C) 2021-2022 Intel Corporation
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
#include "hw_cmds_xe_hpg_core_base.h"

namespace L0 {

template struct CommandListCoreFamily<IGFX_XE_HPG_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPG_CORE>;

} // namespace L0
