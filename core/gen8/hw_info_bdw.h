/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen8.h"

namespace NEO {

struct BDW;

template <>
struct HwMapper<IGFX_BROADWELL> {
    enum { gfxFamily = IGFX_GEN8_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef BDW GfxProduct;
};
} // namespace NEO
