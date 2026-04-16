/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/xe3p_core/cmdlist_xe3p_core.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_NVL, CommandListProductFamily<IGFX_NVL>>
    populateNVL;

static CommandListImmediatePopulateFactory<IGFX_NVL, CommandListImmediateProductFamily<IGFX_NVL>>
    populateNVLImmediate;

} // namespace L0
