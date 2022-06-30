/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hp_core/hw_cmds_base.h"

#include "level_zero/core/source/xe_hp_core/cmdlist_xe_hp_core.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_XE_HP_SDV, CommandListProductFamily<IGFX_XE_HP_SDV>>
    populateXEHP;

static CommandListImmediatePopulateFactory<IGFX_XE_HP_SDV, CommandListImmediateProductFamily<IGFX_XE_HP_SDV>>
    populateXEHPImmediate;

} // namespace L0
