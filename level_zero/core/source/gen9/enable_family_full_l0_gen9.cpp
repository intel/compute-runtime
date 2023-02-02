/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

using Family = Gen9Family;

struct EnableL0Gen9 {
    EnableL0Gen9() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Gen9 enable;
} // namespace NEO
