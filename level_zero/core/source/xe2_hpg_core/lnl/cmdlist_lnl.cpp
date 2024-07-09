/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/xe2_hpg_core/cmdlist_xe2_hpg_core.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_LUNARLAKE, CommandListProductFamily<IGFX_LUNARLAKE>>
    populateLNL;

static CommandListImmediatePopulateFactory<IGFX_LUNARLAKE, CommandListImmediateProductFamily<IGFX_LUNARLAKE>>
    populateLNLImmediate;

} // namespace L0
