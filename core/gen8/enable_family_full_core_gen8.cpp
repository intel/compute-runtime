/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen8/hw_cmds.h"
#include "core/helpers/hw_helper.h"

#include <type_traits>

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef BDWFamily Family;
static const auto gfxFamily = IGFX_GEN8_CORE;

struct EnableCoreGen8 {
    EnableCoreGen8() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreGen8 enable;
} // namespace NEO
