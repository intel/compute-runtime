/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_ROCKETLAKE, CommandListProductFamily<IGFX_ROCKETLAKE>>
    populateRKL;

static CommandListImmediatePopulateFactory<IGFX_ROCKETLAKE, CommandListImmediateProductFamily<IGFX_ROCKETLAKE>>
    populateRKLImmediate;

} // namespace L0
