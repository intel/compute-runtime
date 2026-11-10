/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct Xe2HpgCoreFamily;

using Family = Xe2HpgCoreFamily;

struct EnableL0Xe2HpgCore {
    EnableL0Xe2HpgCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0Xe2HpgCore enable;
} // namespace NEO
