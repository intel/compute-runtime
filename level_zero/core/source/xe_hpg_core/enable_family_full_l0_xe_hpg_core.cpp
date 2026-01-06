/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

using Family = XeHpgCoreFamily;

struct EnableL0XeHpgCore {
    EnableL0XeHpgCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0XeHpgCore enable;
} // namespace NEO
