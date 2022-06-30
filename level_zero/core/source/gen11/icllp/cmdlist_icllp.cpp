/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds_base.h"

#include "level_zero/core/source/gen11/cmdlist_gen11.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_ICELAKE_LP, CommandListProductFamily<IGFX_ICELAKE_LP>>
    populateICLLP;

static CommandListImmediatePopulateFactory<IGFX_ICELAKE_LP, CommandListImmediateProductFamily<IGFX_ICELAKE_LP>>
    populateICLLPImmediate;

} // namespace L0
