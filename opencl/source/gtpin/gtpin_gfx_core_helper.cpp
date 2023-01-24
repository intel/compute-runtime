/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/gtpin/gtpin_gfx_core_helper.h"

namespace NEO {
GTPinGfxCoreHelper *gtpinGfxCoreHelperFactory[IGFX_MAX_CORE] = {};

GTPinGfxCoreHelper &GTPinGfxCoreHelper::get(GFXCORE_FAMILY gfxCore) {
    return *gtpinGfxCoreHelperFactory[gfxCore];
}
} // namespace NEO
