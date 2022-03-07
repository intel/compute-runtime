/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/xe_hpg_core/cmdlist_xe_hpg_core.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_DG2, CommandListProductFamily<IGFX_DG2>>
    populateDG2;

static CommandListImmediatePopulateFactory<IGFX_DG2, CommandListImmediateProductFamily<IGFX_DG2>>
    populateDG2Immediate;

} // namespace L0
