/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_nvlp.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_XE3P_CORE>;
static CommandQueuePopulateFactory<IGFX_NVL, CommandQueueHw<IGFX_XE3P_CORE>>
    populateNVL;

} // namespace L0
