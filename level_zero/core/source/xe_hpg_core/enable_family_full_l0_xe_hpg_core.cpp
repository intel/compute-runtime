/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct XeHpgCoreFamily;

using Family = XeHpgCoreFamily;

struct EnableL0XeHpgCore {
    EnableL0XeHpgCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0XeHpgCore enable;
} // namespace NEO
