/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/cl_hw_helper.h"

namespace NEO {

ClHwHelper *clHwHelperFactory[IGFX_MAX_CORE] = {};

ClHwHelper &ClHwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *clHwHelperFactory[gfxCore];
}

} // namespace NEO
