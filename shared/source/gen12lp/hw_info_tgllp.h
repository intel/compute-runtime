/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen12lp.h"

namespace NEO {

struct TGLLP;

template <>
struct HwMapper<IGFX_TIGERLAKE_LP> {
    enum { gfxFamily = IGFX_GEN12LP_CORE };

    static const char *abbreviation;
    typedef GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily GfxFamily;
    typedef TGLLP GfxProduct;
};
} // namespace NEO
