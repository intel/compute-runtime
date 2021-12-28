/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gen9/cmdlist_gen9.h"

namespace L0 {

static CommandListPopulateFactory<IGFX_COFFEELAKE, CommandListProductFamily<IGFX_COFFEELAKE>>
    populateCFL;

static CommandListImmediatePopulateFactory<IGFX_COFFEELAKE, CommandListImmediateProductFamily<IGFX_COFFEELAKE>>
    populateCFLImmediate;
} // namespace L0
