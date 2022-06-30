/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen9/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_hw_base.inl"

#include "cmdqueue_extended.inl"
#include "igfxfmid.h"

namespace L0 {
template struct CommandQueueHw<IGFX_GEN9_CORE>;
static CommandQueuePopulateFactory<IGFX_KABYLAKE, CommandQueueHw<IGFX_GEN9_CORE>> populateKBL;

} // namespace L0
