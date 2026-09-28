/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE2_HPG_CORE;

template struct CommandQueueHw<gfxCoreFamily>;
static CommandQueuePopulateFactory<gfxCoreFamily, CommandQueueHw<gfxCoreFamily>> populateXe2HpgCore;

} // namespace L0
