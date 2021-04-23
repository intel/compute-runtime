/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef XeHpFamily Family;
static auto gfxFamily = IGFX_XE_HP_CORE;

struct EnableCoreXeHpCore {
    EnableCoreXeHpCore() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreXeHpCore enable;
} // namespace NEO
