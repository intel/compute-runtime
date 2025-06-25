/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe_hpg_core/mutable_cmdlist_xe_hpg_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_DG2, MutableCommandListProductFamily<IGFX_DG2>>
    populateMutableDG2;
} // namespace L0::MCL
