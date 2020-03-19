/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/gen11/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_base.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/gen11/cmdlist_gen11.inl"

#include "cmdlist_extended.inl"
#include "igfxfmid.h"

namespace L0 {

template struct CommandListCoreFamily<IGFX_GEN11_CORE>;
template struct CommandListCoreFamilyImmediate<IGFX_GEN11_CORE>;

template <>
struct CommandListProductFamily<IGFX_ICELAKE_LP> : public CommandListCoreFamily<IGFX_GEN11_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;
};

static CommandListPopulateFactory<IGFX_ICELAKE_LP, CommandListProductFamily<IGFX_ICELAKE_LP>>
    populateICLLP;

template <>
struct CommandListImmediateProductFamily<IGFX_ICELAKE_LP> : public CommandListCoreFamilyImmediate<IGFX_GEN11_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};

static CommandListImmediatePopulateFactory<IGFX_ICELAKE_LP, CommandListImmediateProductFamily<IGFX_ICELAKE_LP>>
    populateICLLPImmediate;

} // namespace L0
