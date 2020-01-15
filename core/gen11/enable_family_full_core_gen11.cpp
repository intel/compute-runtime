/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen11/hw_cmds.h"
#include "core/helpers/hw_helper.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef ICLFamily Family;
static auto gfxFamily = IGFX_GEN11_CORE;

struct EnableCoreGen11 {
    EnableCoreGen11() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreGen11 enable;
} // namespace NEO
