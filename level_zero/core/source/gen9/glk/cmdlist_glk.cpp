/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen9/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_base.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/gen9/cmdlist_gen9.h"
#include "level_zero/core/source/gen9/cmdlist_gen9.inl"

#include "cmdlist_extended.inl"
#include "igfxfmid.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_GEMINILAKE, CommandListProductFamily<IGFX_GEMINILAKE>>
    populateGLK;

static CommandListImmediatePopulateFactory<IGFX_GEMINILAKE, CommandListImmediateProductFamily<IGFX_GEMINILAKE>>
    populateGLKImmediate;

} // namespace L0
