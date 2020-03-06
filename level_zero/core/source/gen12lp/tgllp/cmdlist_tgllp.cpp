/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist_hw_base.inl"
#include "level_zero/core/source/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

#include "cmdlist_extended.inl"
#include "igfxfmid.h"

namespace L0 {
template struct CommandListCoreFamily<IGFX_GEN12LP_CORE>;

static CommandListPopulateFactory<IGFX_TIGERLAKE_LP, CommandListProductFamily<IGFX_TIGERLAKE_LP>>
    populateTGLLP;

static CommandListImmediatePopulateFactory<IGFX_TIGERLAKE_LP, CommandListImmediateProductFamily<IGFX_TIGERLAKE_LP>>
    populateTGLLPImmediate;
} // namespace L0
