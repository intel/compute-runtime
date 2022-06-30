/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

#include "level_zero/core/source/helpers/l0_populate_factory.h"
#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace NEO {

using Family = TGLLPFamily;

struct EnableL0Gen12LP {
    EnableL0Gen12LP() {
        L0::populateFactoryTable<L0::L0HwHelperHw<Family>>();
    }
};

static EnableL0Gen12LP enable;
} // namespace NEO
