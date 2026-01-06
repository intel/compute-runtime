/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe3p_core/mutable_cmdlist_xe3p_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_CRI, MutableCommandListProductFamily<IGFX_CRI>>
    populateMutableCRI;
} // namespace L0::MCL
