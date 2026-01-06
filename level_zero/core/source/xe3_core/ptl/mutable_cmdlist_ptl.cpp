/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source//xe3_core/hw_info_xe3_core.h"
#include "shared/source/xe3_core/hw_cmds_base.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe3_core/mutable_cmdlist_xe3_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_PTL, MutableCommandListProductFamily<IGFX_PTL>>
    populateMutablePTL;
} // namespace L0::MCL
