/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/xe3p_core/cmdlist_xe3p_core.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_CRI, CommandListProductFamily<IGFX_CRI>>
    populateCRI;

static CommandListImmediatePopulateFactory<IGFX_CRI, CommandListImmediateProductFamily<IGFX_CRI>>
    populateCRIImmediate;

} // namespace L0
