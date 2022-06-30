/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/xe_hpc_core/cmdlist_xe_hpc_core.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_PVC, CommandListProductFamily<IGFX_PVC>>
    populatePVC;

static CommandListImmediatePopulateFactory<IGFX_PVC, CommandListImmediateProductFamily<IGFX_PVC>>
    populatePVCImmediate;
} // namespace L0
