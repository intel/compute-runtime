/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_skl_to_tgllp.inl"
#include "level_zero/core/source/gen12lp/definitions/cache_flush_gen12lp.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template struct CommandListCoreFamily<IGFX_GEN12LP_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_GEN12LP_CORE>;

} // namespace L0
