/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct Gen12LpFamily;

using Family = Gen12LpFamily;

struct EnableL0Gen12LP {
    EnableL0Gen12LP() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Gen12LP enable;
} // namespace NEO
