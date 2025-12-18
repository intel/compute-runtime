/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_info_xe3_core.h"

#include "level_zero/core/source/xe3_core/cmdlist_xe3_core.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_NVL_XE3G, CommandListProductFamily<IGFX_NVL_XE3G>>
    populateNVLS;

static CommandListImmediatePopulateFactory<IGFX_NVL_XE3G, CommandListImmediateProductFamily<IGFX_NVL_XE3G>>
    populateNVLSImmediate;

} // namespace L0
