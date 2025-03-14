/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/prefetch_manager.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_dg2_and_pvc.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_gen12lp_to_xe3.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xe_hpc_and_later.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template struct CommandListCoreFamily<IGFX_XE_HPC_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_XE_HPC_CORE>;

} // namespace L0
