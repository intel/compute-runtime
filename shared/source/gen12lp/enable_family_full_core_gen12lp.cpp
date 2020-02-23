/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/hw_helper.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef TGLLPFamily Family;
static auto gfxFamily = IGFX_GEN12LP_CORE;

struct EnableCoreGen12LP {
    EnableCoreGen12LP() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreGen12LP enable;
} // namespace NEO
