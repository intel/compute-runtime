/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gen9/cmdlist_gen9.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_SKYLAKE, CommandListProductFamily<IGFX_SKYLAKE>>
    populateSKL;

static CommandListImmediatePopulateFactory<IGFX_SKYLAKE, CommandListImmediateProductFamily<IGFX_SKYLAKE>>
    populateSKLImmediate;

} // namespace L0
