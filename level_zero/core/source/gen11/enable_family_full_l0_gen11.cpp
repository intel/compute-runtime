/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace NEO {

using Family = ICLFamily;

struct EnableL0Gen11 {
    EnableL0Gen11() {
        L0::populateFactoryTable<L0::L0HwHelperHw<Family>>();
    }
};

static EnableL0Gen11 enable;
} // namespace NEO
