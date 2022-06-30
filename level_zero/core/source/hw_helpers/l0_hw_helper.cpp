/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"

namespace L0 {

L0HwHelper *l0HwHelperFactory[IGFX_MAX_CORE] = {};

L0HwHelper &L0HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *l0HwHelperFactory[gfxCore];
}

} // namespace L0
