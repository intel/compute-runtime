/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/source/xe_hpg_core/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

namespace L0 {
template struct CommandQueueHw<IGFX_XE_HPG_CORE>;
static CommandQueuePopulateFactory<IGFX_DG2, CommandQueueHw<IGFX_XE_HPG_CORE>>
    populateDG2;

} // namespace L0
