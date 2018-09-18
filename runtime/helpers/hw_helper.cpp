/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/hw_helper.h"

namespace OCLRT {
HwHelper *hwHelperFactory[IGFX_MAX_CORE] = {};

HwHelper &HwHelper::get(GFXCORE_FAMILY gfxCore) {
    return *hwHelperFactory[gfxCore];
}
} // namespace OCLRT
