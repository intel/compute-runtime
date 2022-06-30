/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "hw_info_gen12lp.h"

namespace NEO {

struct ADLS;

template <>
struct HwMapper<IGFX_ALDERLAKE_S> {
    enum { gfxFamily = IGFX_GEN12LP_CORE };

    static const char *abbreviation;
    using GfxFamily = GfxFamilyMapper<static_cast<GFXCORE_FAMILY>(gfxFamily)>::GfxFamily;
    using GfxProduct = ADLS;
};
} // namespace NEO
