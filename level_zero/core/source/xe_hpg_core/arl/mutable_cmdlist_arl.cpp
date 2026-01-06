/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe_hpg_core/mutable_cmdlist_xe_hpg_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_ARROWLAKE, MutableCommandListProductFamily<IGFX_ARROWLAKE>>
    populateMutableARL;
} // namespace L0::MCL
