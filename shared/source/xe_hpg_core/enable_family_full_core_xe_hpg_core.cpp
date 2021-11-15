/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"

namespace NEO {

extern HwHelper *hwHelperFactory[IGFX_MAX_CORE];

typedef XE_HPG_COREFamily Family;
static auto gfxFamily = IGFX_XE_HPG_CORE;

struct EnableCoreXeHpgCore {
    EnableCoreXeHpgCore() {
        hwHelperFactory[gfxFamily] = &HwHelperHw<Family>::get();
    }
};

static EnableCoreXeHpgCore enable;
} // namespace NEO
