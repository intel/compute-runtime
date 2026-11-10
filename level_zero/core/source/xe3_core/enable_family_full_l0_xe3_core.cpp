/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct Xe3CoreFamily;

using Family = Xe3CoreFamily;

struct EnableL0Xe3Core {
    EnableL0Xe3Core() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Xe3Core enable;
} // namespace NEO
