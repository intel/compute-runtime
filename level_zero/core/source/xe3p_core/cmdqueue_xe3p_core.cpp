/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_XE3P_CORE;

template struct CommandQueueHw<gfxCoreFamily>;
static CommandQueuePopulateFactory<gfxCoreFamily, CommandQueueHw<gfxCoreFamily>> populateXe3pCore;

} // namespace L0
