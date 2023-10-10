/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_ARROWLAKE, CommandListProductFamily<IGFX_ARROWLAKE>>
    populateARL;

static CommandListImmediatePopulateFactory<IGFX_ARROWLAKE, CommandListImmediateProductFamily<IGFX_ARROWLAKE>>
    populateARLImmediate;

} // namespace L0
