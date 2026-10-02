/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE_HPG_CORE;

template struct CommandQueueHw<gfxCoreFamily>;
static CommandQueuePopulateFactory<gfxCoreFamily, CommandQueueHw<gfxCoreFamily>> populateXeHpgCore;

} // namespace L0
