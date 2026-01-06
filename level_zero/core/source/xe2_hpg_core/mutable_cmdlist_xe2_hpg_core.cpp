/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw_from_xe_hpg_to_xe3.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_command_walker_hw_from_xe_hpg_to_xe3.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_load_register_imm_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_pipe_control_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_semaphore_wait_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_data_imm_hw.inl"
#include "level_zero/core/source/mutable_cmdlist/mutable_store_register_mem_hw.inl"

namespace L0::MCL {
using Family = NEO::Xe2HpgCoreFamily;

template struct MutableCommandListCoreFamily<IGFX_XE2_HPG_CORE>;
template struct MutableComputeWalkerHw<Family>;
template struct MutableLoadRegisterImmHw<Family>;
template struct MutablePipeControlHw<Family>;
template struct MutableSemaphoreWaitHw<Family>;
template struct MutableStoreDataImmHw<Family>;
template struct MutableStoreRegisterMemHw<Family>;

static_assert(NEO::NonCopyableAndNonMovable<MutableComputeWalkerHw<Family>>);

} // namespace L0::MCL
