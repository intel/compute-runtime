/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_ROCKETLAKE, CommandListProductFamily<IGFX_ROCKETLAKE>>
    populateRKL;

static CommandListImmediatePopulateFactory<IGFX_ROCKETLAKE, CommandListImmediateProductFamily<IGFX_ROCKETLAKE>>
    populateRKLImmediate;

} // namespace L0