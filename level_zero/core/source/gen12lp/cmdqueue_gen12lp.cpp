/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_gen12lp.inl"

namespace L0 {

static constexpr auto gfxCoreFamily = IGFX_GEN12LP_CORE;

template struct CommandQueueHw<gfxCoreFamily>;
static CommandQueuePopulateFactory<gfxCoreFamily, CommandQueueHw<gfxCoreFamily>> populateGen12Lp;

} // namespace L0
