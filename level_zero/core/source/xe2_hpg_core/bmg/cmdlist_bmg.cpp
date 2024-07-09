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

static CommandListPopulateFactory<IGFX_BMG, CommandListProductFamily<IGFX_BMG>>
    populateBMG;

static CommandListImmediatePopulateFactory<IGFX_BMG, CommandListImmediateProductFamily<IGFX_BMG>>
    populateBMGImmediate;

} // namespace L0
