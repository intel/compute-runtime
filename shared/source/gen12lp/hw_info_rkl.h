/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen12lp.h"

namespace NEO {

struct RKL;

template <>
struct HwMapper<IGFX_ROCKETLAKE> {
    enum { gfxFamily = IGFX_GEN12LP_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef RKL GfxProduct;
};
} // namespace NEO
