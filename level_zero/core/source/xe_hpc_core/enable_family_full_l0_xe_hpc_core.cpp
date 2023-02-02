/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

using Family = XeHpcCoreFamily;

struct EnableL0XeHpcCore {
    EnableL0XeHpcCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0XeHpcCore enable;
} // namespace NEO
