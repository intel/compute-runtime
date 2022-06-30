/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/source/xe_hp_core/hw_info.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.inl"
#include "level_zero/core/source/cmdqueue/cmdqueue_xe_hp_core_and_later.inl"

#include "cmdqueue_extended.inl"
namespace L0 {
template struct CommandQueueHw<IGFX_XE_HP_CORE>;
static CommandQueuePopulateFactory<IGFX_XE_HP_SDV, CommandQueueHw<IGFX_XE_HP_CORE>>
    populateXEHP;

} // namespace L0
