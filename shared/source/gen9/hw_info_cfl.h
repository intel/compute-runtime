/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen9.h"

namespace NEO {

struct CFL;

template <>
struct HwMapper<IGFX_COFFEELAKE> {
    enum { gfxFamily = IGFX_GEN9_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef CFL GfxProduct;
};
} // namespace NEO
