/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_ALDERLAKE_N, CommandListProductFamily<IGFX_ALDERLAKE_N>>
    populateADLN;

static CommandListImmediatePopulateFactory<IGFX_ALDERLAKE_N, CommandListImmediateProductFamily<IGFX_ALDERLAKE_N>>
    populateADLNImmediate;

} // namespace L0
