/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct XeHpcCoreFamily;

using Family = XeHpcCoreFamily;

struct EnableL0XeHpcCore {
    EnableL0XeHpcCore() {
        L0::populateFactoryTable<L0::L0GfxCoreHelperHw<Family>>();
    }
};

static EnableL0XeHpcCore enable;
} // namespace NEO
