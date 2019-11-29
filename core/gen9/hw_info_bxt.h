/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen9.h"

namespace NEO {

struct BXT;

template <>
struct HwMapper<IGFX_BROXTON> {
    enum { gfxFamily = IGFX_GEN9_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef BXT GfxProduct;
};
} // namespace NEO
