/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"

#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist_hw.inl"
#include "level_zero/core/source/xe3_core/mutable_cmdlist_xe3_core.h"

namespace L0::MCL {
static MutableCommandListPopulateFactory<IGFX_NVL_XE3G, MutableCommandListProductFamily<IGFX_NVL_XE3G>>
    populateMutableNVLS;
} // namespace L0::MCL
