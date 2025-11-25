/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"

#include "hw_cmds_xe3p_core.h"

namespace NEO {

using Family = Xe3pCoreFamily;

struct EnableL0Xe3pCore {
    EnableL0Xe3pCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Xe3pCore enable;
} // namespace NEO
