/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_METEORLAKE, CommandListProductFamily<IGFX_METEORLAKE>>
    populateMTL;

static CommandListImmediatePopulateFactory<IGFX_METEORLAKE, CommandListImmediateProductFamily<IGFX_METEORLAKE>>
    populateMTLImmediate;

} // namespace L0
