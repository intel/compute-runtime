/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct Xe3pCoreFamily;

using Family = Xe3pCoreFamily;

struct EnableL0Xe3pCore {
    EnableL0Xe3pCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Xe3pCore enable;
} // namespace NEO
