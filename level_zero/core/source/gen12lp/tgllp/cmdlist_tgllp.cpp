/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {
static CommandListPopulateFactory<IGFX_TIGERLAKE_LP, CommandListProductFamily<IGFX_TIGERLAKE_LP>>
    populateTGLLP;

static CommandListImmediatePopulateFactory<IGFX_TIGERLAKE_LP, CommandListImmediateProductFamily<IGFX_TIGERLAKE_LP>>
    populateTGLLPImmediate;
} // namespace L0
