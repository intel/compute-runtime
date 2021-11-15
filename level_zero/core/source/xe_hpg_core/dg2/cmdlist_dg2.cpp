/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "cmdlist_dg2.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.inl"
#include "level_zero/core/source/cmdlist/cmdlist_hw_xehp_and_later.inl"

#include "cmdlist_extended.inl"

namespace L0 {

template struct CommandListCoreFamily<IGFX_XE_HPG_CORE>;

static CommandListPopulateFactory<IGFX_DG2, CommandListProductFamily<IGFX_DG2>>
    populateDG2;

static CommandListImmediatePopulateFactory<IGFX_DG2, CommandListImmediateProductFamily<IGFX_DG2>>
    populateDG2Immediate;

} // namespace L0
