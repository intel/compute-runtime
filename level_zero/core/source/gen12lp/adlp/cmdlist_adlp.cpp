/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_ALDERLAKE_P, CommandListProductFamily<IGFX_ALDERLAKE_P>>
    populateADLP;

static CommandListImmediatePopulateFactory<IGFX_ALDERLAKE_P, CommandListImmediateProductFamily<IGFX_ALDERLAKE_P>>
    populateADLPImmediate;

} // namespace L0
