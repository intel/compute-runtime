/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace NEO {

using Family = XE_HPG_COREFamily;

struct EnableL0XeHpgCore {
    EnableL0XeHpgCore() {
        L0::populateFactoryTable<L0::L0HwHelperHw<Family>>();
    }
};

static EnableL0XeHpgCore enable;
} // namespace NEO
