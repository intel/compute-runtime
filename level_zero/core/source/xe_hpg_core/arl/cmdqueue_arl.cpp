/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_arl.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_XE_HPG_CORE>;
static CommandQueuePopulateFactory<IGFX_ARROWLAKE, CommandQueueHw<IGFX_XE_HPG_CORE>>
    populateARL;

} // namespace L0
