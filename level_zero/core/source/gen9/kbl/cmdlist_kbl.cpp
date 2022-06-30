/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gen9/cmdlist_gen9.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_KABYLAKE, CommandListProductFamily<IGFX_KABYLAKE>>
    populateKBL;

static CommandListImmediatePopulateFactory<IGFX_KABYLAKE, CommandListImmediateProductFamily<IGFX_KABYLAKE>>
    populateKBLImmediate;
} // namespace L0
