/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/gtpin/gtpin_hw_helper.h"

namespace NEO {
GTPinHwHelper *gtpinHwHelperFactory[IGFX_MAX_CORE] = {};

GTPinHwHelper &GTPinHwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *gtpinHwHelperFactory[gfxCore];
}
} // namespace NEO
