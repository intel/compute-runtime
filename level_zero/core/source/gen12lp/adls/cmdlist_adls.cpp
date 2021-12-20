/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gen12lp/cmdlist_gen12lp.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_ALDERLAKE_S, CommandListProductFamily<IGFX_ALDERLAKE_S>>
    populateADLS;

static CommandListImmediatePopulateFactory<IGFX_ALDERLAKE_S, CommandListImmediateProductFamily<IGFX_ALDERLAKE_S>>
    populateADLSImmediate;

} // namespace L0