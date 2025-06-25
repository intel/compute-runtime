/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe2_hpg_core/mutable_cmdlist_xe2_hpg_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_BMG, MutableCommandListProductFamily<IGFX_BMG>>
    populateMutableBMG;
} // namespace L0::MCL
