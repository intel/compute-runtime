/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_DG1, CommandListProductFamily<IGFX_DG1>>
    populateDG1;

static CommandListImmediatePopulateFactory<IGFX_DG1, CommandListImmediateProductFamily<IGFX_DG1>>
    populateDG1Immediate;

} // namespace L0
